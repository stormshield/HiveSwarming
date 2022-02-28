// (C) Stormshield 2022
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "Conversions.h"
#include "CommonFunctions.h"
#include <sstream>

/// @brief Create an internal representation of a registry key from the handle to a registry key
/// @param[in] Hkey the registry key
/// @param[in] KeyName Name of the key #Hkey is a handle to
/// @param[out] RegKey Representation of the key
/// @return HRESULT semantics
_Must_inspect_result_
static HRESULT HkeyToInternal
(
    _In_ const HKEY Hkey,
    _In_ const std::wstring& KeyName,
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

    Result = HRESULT_FROM_WIN32(RegQueryInfoKeyW(Hkey, NULL, 0, NULL, &SubkeyCount, &MaximalSubKeyLength,
        NULL, &ValueCount, &MaximalValueNameLength, &MaximalValueLength, NULL, NULL));
    if (FAILED(Result))
    {
        ReportError(Result, L"Getting information on HKEY - Current key name: " + KeyName);
        goto Cleanup;
    }

    SubKeyNameBuffer = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (1 + MaximalSubKeyLength) * sizeof(WCHAR));
    if (SubKeyNameBuffer == NULL)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Allocating space for subkey names - Current key name: " + KeyName);
        goto Cleanup;
    }

    ValueNameBuffer = (LPWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (1 + MaximalValueNameLength) * sizeof(WCHAR));
    if (ValueNameBuffer == NULL)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Allocating space for value names - Current key name: " + KeyName);
        goto Cleanup;
    }

    ValueBuffer = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MaximalValueLength);
    if (ValueBuffer == NULL)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Allocating space for values - Current key name: " + KeyName);
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

        RegistryValue NewValue;
        NewValue.Name.assign(ValueNameBuffer, ValueNameLength);
        NewValue.Type = ValueType;
        NewValue.BinaryValue.assign(ValueBuffer, ValueBuffer + DataLength);

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

        std::wstring SubkeyName;
        SubkeyName.assign(SubKeyNameBuffer, SubkeyNameLength);

        RegistryKey NewKey;
        Result = HkeyToInternal(HSubkey, SubkeyName, NewKey);
        if (FAILED(Result))
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Getting contents of subkey named " << SubkeyName << L" - Current key name: " << KeyName;
            ReportError(Result, ErrorMessageStream.str());
            goto Cleanup;
        }
        RegKey.Subkeys.emplace_back(std::move(NewKey));

        RegCloseKey(HSubkey);
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

// non-static function: documented in header.
_Must_inspect_result_
HRESULT HiveToInternal
(
    _In_ const std::wstring& HiveFilePath,
    _In_ const std::wstring& RootName,
    _Out_ RegistryKey& RegKey
)
{
    HRESULT Result = E_FAIL;
    HKEY HiveKey = NULL;

    Result = HRESULT_FROM_WIN32(RegLoadAppKeyW(HiveFilePath.c_str(), &HiveKey, KEY_ALL_ACCESS, REG_PROCESS_APPKEY, 0));
    if (FAILED(Result))
    {
        ReportError(Result, L"Loading hive file " + HiveFilePath);
        goto Cleanup;
    }

    Result = HkeyToInternal(HiveKey, RootName, RegKey);

Cleanup:
    if (HiveKey != NULL)
    {
        RegCloseKey(HiveKey);
        HiveKey = NULL;

        DeleteHiveLogFiles(HiveFilePath);
    }


    return Result;
}