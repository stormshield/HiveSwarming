// (C) Stormshield 2025
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "Conversions.h"
#include "CommonFunctions.h"
#include <sstream>
#include <iomanip>
#include "Constants.h"
#include <windows.h>
#include <winternl.h>

/// <summary>
/// Handle to the NTDLL Module, used for calling NtSetValueKey directly
/// </summary>
static HMODULE NtDllModuleHandle = nullptr;

/// <summary>
/// Pointer to the NtSetValueKey function
/// </summary>
static NTSTATUS(*NtSetValueKey)(
    _In_     HANDLE          KeyHandle,
    _In_     PUNICODE_STRING ValueName,
    _In_opt_ ULONG           TitleIndex,
    _In_     ULONG           Type,
    _In_opt_ PVOID           Data,
    _In_     ULONG           DataSize
) = nullptr;

/// <summary>
/// Obtain the address to the NtSetValueKey function
/// </summary>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT LoadNtDllFunctions() {
    if (NtDllModuleHandle == nullptr) {
        NtDllModuleHandle = LoadLibraryExW(L"ntdll.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (NtDllModuleHandle == nullptr) {
            ReportError(HRESULT_FROM_WIN32(GetLastError()), L"Loading ntdll.dll");
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }
    if (NtSetValueKey == nullptr) {
        NtSetValueKey = (decltype(NtSetValueKey)) GetProcAddress(NtDllModuleHandle, "NtSetValueKey");
        if (NtSetValueKey == nullptr) {
            ReportError(HRESULT_FROM_WIN32(GetLastError()), L"Getting address of NtSetValueKey in ntdll.dll");
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }
    return S_OK;
}

/// <summary>
/// Writes the internal representation to a hegistry key handle
/// </summary>
/// <param name="RegKey">Internal representation</param>
/// <param name="KeyHandle">Handle to a registry key with write access</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT InternalToHkey
(
    _In_ RegistryKey const & RegKey,
    _In_ HKEY                KeyHandle
)
{
    HRESULT Result = E_FAIL;
    HKEY SubkeyHandle = NULL;

    if (KeyHandle == NULL)
    {
        ReportError(E_HANDLE, L"Invalid Parameter");
        return E_HANDLE;
    }

    for (auto &Value : RegKey.Values)
    {
        if (NtSetValueKey == nullptr)
        {
            Result = LoadNtDllFunctions();
            if (FAILED(Result))
            {
                ReportError(Result, L"Loading ntdll functions");
                goto Cleanup;
            }
            if (NtSetValueKey == nullptr)
            {
                Result = E_UNEXPECTED;
                ReportError(Result, L"Loading ntdll functions");
                goto Cleanup;
            }
        }

        if (Value.Name.size() >= USHRT_MAX / sizeof(WCHAR))
        {
            ReportError(E_UNEXPECTED, L"Name is too long (name: " + Value.Name + L")");
            Result = E_UNEXPECTED;
            goto Cleanup;
        }
        if (Value.BinaryValue.size() > MAXDWORD)
        {
            ReportError(E_UNEXPECTED, L"Binary value is too long (name: " + Value.Name + L")");
            Result = E_UNEXPECTED;
            goto Cleanup;
        }
        UNICODE_STRING ValueName = {};
        ValueName.Buffer = (PWSTR)Value.Name.c_str();
        ValueName.Length = (USHORT)Value.Name.size() * sizeof(*Value.Name.c_str());
        ValueName.MaximumLength = ValueName.Length;
        NTSTATUS Status = NtSetValueKey(KeyHandle, &ValueName, 0ul, Value.Type, (PVOID)Value.BinaryValue.data(), static_cast<ULONG>(Value.BinaryValue.size()));
        if (NT_SUCCESS(Status) == FALSE)
        {
            ReportError(Status, L"Could not set value " + Value.Name + L" of key " + RegKey.Name + L" with NtSetValueKey, trying fallback");
            goto Cleanup;
        }
    }

    for (auto &Key : RegKey.Subkeys)
    {
        if (SubkeyHandle != NULL)
        {
            RegCloseKey(SubkeyHandle);
            SubkeyHandle = NULL;
        }

        if (Key.Values.size() == 1 && Key.Subkeys.size() == 0 && Key.Values[0].Type == REG_LINK && Key.Values[0].Name == Constants::Hives::SymbolicLinkValue)
        {
            Result = HRESULT_FROM_WIN32(RegCreateKeyExW(KeyHandle, Key.Name.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE | REG_OPTION_CREATE_LINK,
                KEY_ALL_ACCESS, NULL, &SubkeyHandle, NULL));
        }
        else
        {
            Result = HRESULT_FROM_WIN32(RegCreateKeyExW(KeyHandle, Key.Name.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                KEY_ALL_ACCESS, NULL, &SubkeyHandle, NULL));
        }

        if (FAILED(Result))
        {
            ReportError(Result, L"Could not create subkey " + Key.Name + L" of key " + RegKey.Name);
            goto Cleanup;
        }

        Result = InternalToHkey(Key, SubkeyHandle);

        if (FAILED(Result))
        {
            ReportError(Result, L"Could not render subkey " + Key.Name + L" of key " + RegKey.Name);
            goto Cleanup;
        }
    }

    Result = S_OK;
Cleanup:
    if (SubkeyHandle != NULL)
    {
        RegCloseKey(SubkeyHandle);
        SubkeyHandle = NULL;
    }

    return Result;
}

_Must_inspect_result_ HRESULT InternalToHive
(
    _In_ RegistryKey  const & RegKey,
    _In_ std::wstring const & OutputFilePath
)
{
    HRESULT Result = E_FAIL;
    HKEY HiveKey = NULL;

    if (!DeleteFileW(OutputFilePath.c_str()))
    {
        if (GetLastError() != ERROR_FILE_NOT_FOUND)
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            ReportError(Result, L"Could not delete hive file " + OutputFilePath);
            goto Cleanup;
        }
    }

    Result = HRESULT_FROM_WIN32(RegLoadAppKeyW(OutputFilePath.c_str(), &HiveKey, KEY_ALL_ACCESS, REG_PROCESS_APPKEY, 0));
    if (FAILED(Result))
    {
        ReportError(Result, L"Could not create empty hive file " + OutputFilePath);
        goto Cleanup;
    }

    Result = InternalToHkey(RegKey, HiveKey);
    if (FAILED(Result))
    {
        ReportError(Result, L"Could not render internal structure to hive");
    }

    Result = S_OK;

Cleanup:
    if (HiveKey != NULL)
    {
        RegCloseKey(HiveKey);
        HiveKey = NULL;

        DeleteHiveLogFiles(OutputFilePath);
    }

    return Result;
}