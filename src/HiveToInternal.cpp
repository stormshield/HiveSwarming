// (C) Stormshield 2026
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "Conversions.h"
#include "CommonFunctions.h"
#include <sstream>
#include <iostream>

namespace OfflineRegistry {
    using ORHKEY = HANDLE;
    using PORHKEY = ORHKEY*;

    using OROpenHivePrototype = DWORD(*)(_In_ PCWSTR, _Out_ PORHKEY);

    using ORQueryInfoKeyPrototype = DWORD(*)(_In_ ORHKEY, _Out_opt_ PWSTR,
        _Inout_opt_ PDWORD, _Out_opt_ PDWORD, _Out_opt_ PDWORD,
        _Out_opt_ PDWORD, _Out_opt_ PDWORD, _Out_opt_ PDWORD,
        _Out_opt_ PDWORD, _Out_opt_ PDWORD, _Out_opt_ PFILETIME);

    using OREnumKeyPrototype = DWORD(*)(_In_ ORHKEY, _In_ DWORD, _Out_ PWSTR,
        _Inout_ PDWORD, _Out_opt_ PWSTR, _Inout_opt_ PDWORD, _Out_opt_ PFILETIME);

    using OREnumValuePrototype = DWORD(*)(_In_ ORHKEY, _In_ DWORD, _Out_ PWSTR,
        _Inout_ PDWORD, _Out_opt_ PDWORD, _Out_opt_ PBYTE lpData, _Inout_opt_ PDWORD);

    using OROpenKeyPrototype = DWORD(*)(_In_ ORHKEY, _In_opt_ PCWSTR, _Out_ PORHKEY);

    using ORCloseKeyPrototype = DWORD(*)(_In_ ORHKEY);

    using ORCloseHivePrototype = DWORD(*)(_In_ ORHKEY);
    static ORCloseHivePrototype ORCloseHive = nullptr;
    static struct OfflineRegistryFunctionsClass {
        OROpenHivePrototype OROpenHive;
        ORQueryInfoKeyPrototype ORQueryInfoKey;
        OREnumKeyPrototype OREnumKey;
        OREnumValuePrototype OREnumValue;
        OROpenKeyPrototype OROpenKey;
        ORCloseKeyPrototype ORCloseKey;
    } OfflineRegistryFunctions = {};

    /// <summary>
    /// Obtain the address to the offline registry functions
    /// </summary>
    /// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
    _Must_inspect_result_ static HRESULT LoadOfflineRegistryFunctions() {
        static HMODULE OffRegModuleHandle = nullptr;
        if (OfflineRegistryFunctions.ORCloseKey != nullptr) return S_OK;

        if (OffRegModuleHandle == nullptr) {
            OffRegModuleHandle = LoadLibraryExW(L"offreg.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (OffRegModuleHandle == nullptr) {
                ReportError(HRESULT_FROM_WIN32(GetLastError()), L"Loading offreg.dll");
                return HRESULT_FROM_WIN32(GetLastError());
            }
        }

        OfflineRegistryFunctionsClass TmpOfflineRegistryFunctions{};
        TmpOfflineRegistryFunctions.OROpenHive = (decltype(TmpOfflineRegistryFunctions.OROpenHive))GetProcAddress(OffRegModuleHandle, "OROpenHive");
        if (TmpOfflineRegistryFunctions.OROpenHive == nullptr) {
            ReportError(HRESULT_FROM_WIN32(GetLastError()), L"Getting address of OROpenHive in offreg.dll");
            return HRESULT_FROM_WIN32(GetLastError());
        }
        TmpOfflineRegistryFunctions.ORQueryInfoKey = (decltype(TmpOfflineRegistryFunctions.ORQueryInfoKey))GetProcAddress(OffRegModuleHandle, "ORQueryInfoKey");
        if (TmpOfflineRegistryFunctions.ORQueryInfoKey == nullptr) {
            ReportError(HRESULT_FROM_WIN32(GetLastError()), L"Getting address of ORQueryInfoKey in offreg.dll");
            return HRESULT_FROM_WIN32(GetLastError());
        }
        TmpOfflineRegistryFunctions.OREnumKey = (decltype(TmpOfflineRegistryFunctions.OREnumKey))GetProcAddress(OffRegModuleHandle, "OREnumKey");
        if (TmpOfflineRegistryFunctions.OREnumKey == nullptr) {
            ReportError(HRESULT_FROM_WIN32(GetLastError()), L"Getting address of OREnumKey in offreg.dll");
            return HRESULT_FROM_WIN32(GetLastError());
        }
        TmpOfflineRegistryFunctions.OREnumValue = (decltype(TmpOfflineRegistryFunctions.OREnumValue))GetProcAddress(OffRegModuleHandle, "OREnumValue");
        if (TmpOfflineRegistryFunctions.OREnumValue == nullptr) {
            ReportError(HRESULT_FROM_WIN32(GetLastError()), L"Getting address of OREnumValue in offreg.dll");
            return HRESULT_FROM_WIN32(GetLastError());
        }
        TmpOfflineRegistryFunctions.OROpenKey = (decltype(TmpOfflineRegistryFunctions.OROpenKey))GetProcAddress(OffRegModuleHandle, "OROpenKey");
        if (TmpOfflineRegistryFunctions.OROpenKey == nullptr) {
            ReportError(HRESULT_FROM_WIN32(GetLastError()), L"Getting address of OROpenKey in offreg.dll");
            return HRESULT_FROM_WIN32(GetLastError());
        }
        TmpOfflineRegistryFunctions.ORCloseKey = (decltype(TmpOfflineRegistryFunctions.ORCloseKey))GetProcAddress(OffRegModuleHandle, "ORCloseKey");
        if (TmpOfflineRegistryFunctions.ORCloseKey == nullptr) {
            ReportError(HRESULT_FROM_WIN32(GetLastError()), L"Getting address of ORCloseKey in offreg.dll");
            return HRESULT_FROM_WIN32(GetLastError());
        }

        OfflineRegistryFunctions = TmpOfflineRegistryFunctions;
        return S_OK;
    }


    /// <summary>
    /// Convert a registry key to the internal representation
    /// </summary>
    /// <param name="Hkey">Handle to the registry key, with read rights</param>
    /// <param name="KeyName">Name of the key <paramref name="Hkey"/> is a handle to</param>
    /// <param name="RegKey">Internal representation of the key</param>
    /// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
    _Must_inspect_result_ static HRESULT HkeyToInternalUsingOfflineRegistry
    (
        _In_  ORHKEY                   Hkey,
        _In_  std::wstring_view const& KeyName,
        _Out_ RegistryKey& RegKey
    )
    {
        HRESULT Result = E_FAIL;
        DWORD SubkeyCount = 0;
        DWORD MaximalSubKeyLength = 0;
        DWORD ValueCount = 0;
        DWORD MaximalValueNameLength = 0;
        DWORD MaximalValueLength = 0;
        LPWSTR SubKeyNameBuffer = NULL;
        LPWSTR ValueNameBuffer = NULL;
        PBYTE ValueBuffer = NULL;

        if (Hkey == NULL)
        {
            ReportError(E_HANDLE, L"Null HKEY");
            return E_HANDLE;
        }

        Result = HRESULT_FROM_WIN32(OfflineRegistryFunctions.ORQueryInfoKey(Hkey, NULL, 0, &SubkeyCount, &MaximalSubKeyLength,
            NULL, &ValueCount, &MaximalValueNameLength, &MaximalValueLength, NULL, NULL));
        if (FAILED(Result))
        {
            ReportError(Result, L"Getting information on HKEY - Current key name: " + std::wstring{ KeyName });
            goto Cleanup;
        }

        SubKeyNameBuffer = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (1 + (size_t)MaximalSubKeyLength) * sizeof(WCHAR));
        if (SubKeyNameBuffer == NULL)
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            ReportError(Result, std::wstring{ L"Allocating space for subkey names - Current key name: " } + std::wstring{ KeyName });
            goto Cleanup;
        }

        ValueNameBuffer = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (1 + (size_t)MaximalValueNameLength) * sizeof(WCHAR));
        if (ValueNameBuffer == NULL)
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            ReportError(Result, std::wstring{ L"Allocating space for value names - Current key name: " } + std::wstring{ KeyName });
            goto Cleanup;
        }

        ValueBuffer = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MaximalValueLength);
        if (ValueBuffer == NULL)
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            ReportError(Result, L"Allocating space for values - Current key name: " + std::wstring{ KeyName });
            goto Cleanup;
        }

        RegKey.Name = KeyName;

        for (DWORD ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
        {
            DWORD ValueNameLength = MaximalValueNameLength + 1;
            DWORD ValueType = 0;
            DWORD DataLength = MaximalValueLength + 1;
            Result = HRESULT_FROM_WIN32(OfflineRegistryFunctions.OREnumValue(Hkey, ValueIndex, ValueNameBuffer, &ValueNameLength,
                &ValueType, ValueBuffer, &DataLength));
            if (FAILED(Result))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Getting value at index " << ValueIndex << L" - Current key name: " << KeyName;
                ReportError(Result, ErrorMessageStream.str());
                goto Cleanup;
            }

            RegistryValue NewValue{
                .Name       {ValueNameBuffer, ValueNameLength},
                .Type       {ValueType},
                .BinaryValue{ValueBuffer, ValueBuffer + DataLength}
            };

            RegKey.Values.emplace_back(std::move(NewValue));
        }

        for (DWORD SubkeyIndex = 0; SubkeyIndex < SubkeyCount; ++SubkeyIndex)
        {
            ORHKEY HSubkey = NULL;
            DWORD SubkeyNameLength = MaximalSubKeyLength + 1;

            Result = HRESULT_FROM_WIN32(OfflineRegistryFunctions.OREnumKey(Hkey, SubkeyIndex, SubKeyNameBuffer, &SubkeyNameLength, NULL, NULL, NULL));
            if (FAILED(Result))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Getting subkey name at index " << SubkeyIndex << L" - Current key name: " << KeyName;
                ReportError(Result, ErrorMessageStream.str());
                goto Cleanup;
            }

            Result = HRESULT_FROM_WIN32(OfflineRegistryFunctions.OROpenKey(Hkey, SubKeyNameBuffer, &HSubkey));
            if (FAILED(Result))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Opening subkey named " << SubKeyNameBuffer << L" at index " << SubkeyIndex << L" - Current key name: " << KeyName;
                ReportError(Result, ErrorMessageStream.str());
                goto Cleanup;
            }

            std::wstring SubkeyName{ SubKeyNameBuffer, SubkeyNameLength };

            RegistryKey NewKey;
            Result = HkeyToInternalUsingOfflineRegistry(HSubkey, SubkeyName, NewKey); /// ⟵ Recursion here
            OfflineRegistryFunctions.ORCloseKey(HSubkey);
            if (FAILED(Result))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Getting contents of subkey named " << SubkeyName << L" - Current key name: " << KeyName;
                ReportError(Result, ErrorMessageStream.str());
                goto Cleanup;
            }
            RegKey.Subkeys.emplace_back(std::move(NewKey));
        }
    Cleanup:
        if (SubKeyNameBuffer)
        {
            HeapFree(GetProcessHeap(), 0, (LPVOID)SubKeyNameBuffer);
            SubKeyNameBuffer = NULL;
        }
        if (ValueNameBuffer)
        {
            HeapFree(GetProcessHeap(), 0, (LPVOID)ValueNameBuffer);
            ValueNameBuffer = NULL;
        }
        if (ValueBuffer)
        {
            HeapFree(GetProcessHeap(), 0, (LPVOID)ValueBuffer);
            ValueBuffer = NULL;
        }

        return Result;
    }
}


/// <summary>
/// Convert a registry key to the internal representation
/// </summary>
/// <param name="Hkey">Handle to the registry key, with read rights</param>
/// <param name="KeyName">Name of the key <paramref name="Hkey"/> is a handle to</param>
/// <param name="RegKey">Internal representation of the key</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT HkeyToInternal
(
    _In_  HKEY                      Hkey,
    _In_  std::wstring_view const & KeyName,
    _Out_ RegistryKey             & RegKey
)
{
    HRESULT Result = E_FAIL;
    DWORD SubkeyCount = 0;
    DWORD MaximalSubKeyLength = 0;
    DWORD ValueCount = 0;
    DWORD MaximalValueNameLength = 0;
    DWORD MaximalValueLength = 0;
    LPWSTR SubKeyNameBuffer = NULL;
    LPWSTR ValueNameBuffer = NULL;
    PBYTE ValueBuffer = NULL;

    if (Hkey == NULL)
    {
        ReportError(E_HANDLE, L"Null HKEY");
        return E_HANDLE;
    }

    Result = HRESULT_FROM_WIN32(RegQueryInfoKeyW(Hkey, NULL, 0, NULL, &SubkeyCount, &MaximalSubKeyLength,
        NULL, &ValueCount, &MaximalValueNameLength, &MaximalValueLength, NULL, NULL));
    if (FAILED(Result))
    {
        ReportError(Result, L"Getting information on HKEY - Current key name: " + std::wstring{ KeyName });
        goto Cleanup;
    }

    SubKeyNameBuffer = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (1 + (size_t)MaximalSubKeyLength) * sizeof(WCHAR));
    if (SubKeyNameBuffer == NULL)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, std::wstring{ L"Allocating space for subkey names - Current key name: " } + std::wstring{ KeyName });
        goto Cleanup;
    }

    ValueNameBuffer = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (1 + (size_t)MaximalValueNameLength) * sizeof(WCHAR));
    if (ValueNameBuffer == NULL)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, std::wstring{ L"Allocating space for value names - Current key name: " } + std::wstring{ KeyName });
        goto Cleanup;
    }

    ValueBuffer = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MaximalValueLength);
    if (ValueBuffer == NULL)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Allocating space for values - Current key name: " + std::wstring{ KeyName });
        goto Cleanup;
    }

    RegKey.Name = KeyName;

    for (DWORD ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
    {
        DWORD ValueNameLength = MaximalValueNameLength + 1;
        DWORD ValueType = 0;
        DWORD DataLength = MaximalValueLength + 1;
        Result = HRESULT_FROM_WIN32(RegEnumValueW(Hkey, ValueIndex, ValueNameBuffer, &ValueNameLength,
            NULL, &ValueType, ValueBuffer, &DataLength));
        if (FAILED(Result))
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Getting value at index " << ValueIndex << L" - Current key name: " << KeyName;
            ReportError(Result,  ErrorMessageStream.str());
            goto Cleanup;
        }

        RegistryValue NewValue{
            .Name       {ValueNameBuffer, ValueNameLength},
            .Type       {ValueType},
            .BinaryValue{ValueBuffer, ValueBuffer + DataLength}
        };

        RegKey.Values.emplace_back(std::move(NewValue));
    }

    for (DWORD SubkeyIndex = 0; SubkeyIndex < SubkeyCount; ++SubkeyIndex)
    {
        HKEY HSubkey = NULL;
        DWORD SubkeyNameLength = MaximalSubKeyLength + 1;

        Result = HRESULT_FROM_WIN32(RegEnumKeyExW(Hkey, SubkeyIndex, SubKeyNameBuffer, &SubkeyNameLength, NULL, NULL, NULL, NULL));
        if (FAILED(Result))
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Getting subkey name at index " << SubkeyIndex << L" - Current key name: " << KeyName;
            ReportError(Result, ErrorMessageStream.str());
            goto Cleanup;
        }

        Result = HRESULT_FROM_WIN32(RegOpenKeyExW(Hkey, SubKeyNameBuffer, REG_OPTION_OPEN_LINK, KEY_READ, &HSubkey));
        if (FAILED(Result)) {
            Result = HRESULT_FROM_WIN32(RegOpenKeyExW(Hkey, SubKeyNameBuffer, 0, KEY_READ, &HSubkey));
        }
        if (FAILED(Result))
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Opening subkey named " << SubKeyNameBuffer << L" at index " << SubkeyIndex << L" - Current key name: " << KeyName;
            ReportError(Result, ErrorMessageStream.str());
            goto Cleanup;
        }

        std::wstring SubkeyName{ SubKeyNameBuffer, SubkeyNameLength };

        RegistryKey NewKey;
        Result = HkeyToInternal(HSubkey, SubkeyName, NewKey); /// ⟵ Recursion here
        RegCloseKey(HSubkey);
        if (FAILED(Result))
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Getting contents of subkey named " << SubkeyName << L" - Current key name: " << KeyName;
            ReportError(Result, ErrorMessageStream.str());
            goto Cleanup;
        }
        RegKey.Subkeys.emplace_back(std::move(NewKey));
    }
Cleanup:
    if (SubKeyNameBuffer)
    {
        HeapFree(GetProcessHeap(), 0, (LPVOID)SubKeyNameBuffer);
        SubKeyNameBuffer = NULL;
    }
    if (ValueNameBuffer)
    {
        HeapFree(GetProcessHeap(), 0, (LPVOID)ValueNameBuffer);
        ValueNameBuffer = NULL;
    }
    if (ValueBuffer)
    {
        HeapFree(GetProcessHeap(), 0, (LPVOID)ValueBuffer);
        ValueBuffer = NULL;
    }

    return Result;
}

_Must_inspect_result_ static HRESULT HiveToInternalUsingOfflineRegistry
(
    _In_  std::wstring      const& HiveFilePath,
    _In_  std::wstring_view const& RootName,
    _Out_ RegistryKey& RegKey
)
{
    HRESULT Result = E_FAIL;
    OfflineRegistry::ORHKEY HiveKey = NULL;
    Result = OfflineRegistry::LoadOfflineRegistryFunctions();
    if (FAILED(Result))
    {
        ReportError(Result, L"Loading offline registry functions");
        goto Cleanup;
    }

    Result = HRESULT_FROM_WIN32(OfflineRegistry::OfflineRegistryFunctions.OROpenHive(HiveFilePath.c_str(), &HiveKey));
    if (FAILED(Result))
    {
        ReportError(Result, L"Loading hive file " + std::wstring{ HiveFilePath });
        goto Cleanup;
    }

    Result = OfflineRegistry::HkeyToInternalUsingOfflineRegistry(HiveKey, RootName, RegKey);

Cleanup:
    if (HiveKey != NULL)
    {
        OfflineRegistry::OfflineRegistryFunctions.ORCloseKey(HiveKey);
        HiveKey = NULL;

        DeleteHiveLogFiles(HiveFilePath);
    }

    return Result;
}

_Must_inspect_result_ HRESULT HiveToInternal
(
    _In_  std::wstring      const & HiveFilePath,
    _In_  std::wstring_view const & RootName,
    _Out_ RegistryKey             & RegKey
)
{
    HRESULT Result = E_FAIL;
    HKEY HiveKey = NULL;
    Result = HRESULT_FROM_WIN32(RegLoadAppKeyW(HiveFilePath.c_str(), &HiveKey, KEY_READ, REG_PROCESS_APPKEY, 0));
    if (FAILED(Result))
    {
        ReportError(Result, L"Loading hive file (using RegLoadAppKeyW) " + std::wstring{ HiveFilePath });
        std::wcerr << L"Falling back to Offline Registry Library..." << std::endl;
        Result = HiveToInternalUsingOfflineRegistry(HiveFilePath, RootName, RegKey);
    }
    else
    {
        Result = HkeyToInternal(HiveKey, RootName, RegKey);
    }

    if (HiveKey != NULL)
    {
        RegCloseKey(HiveKey);
        HiveKey = NULL;

        DeleteHiveLogFiles(HiveFilePath);
    }

    return Result;
}