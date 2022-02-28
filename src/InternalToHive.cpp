// (C) Stormshield 2022
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "Conversions.h"
#include "CommonFunctions.h"
#include <sstream>
#include <iomanip>
#include "Constants.h"

/// @brief Fill an empty HKEY with internal structures representing a registry key and dependencies
/// @param[in] RegKey Registry key to dump into the HKEY
/// @param[in] KeyHandle Handle to the key
/// @return HRESULT semantics
_Must_inspect_result_
static HRESULT InternalToHkey(
    _In_ const RegistryKey& RegKey,
    _In_ const HKEY KeyHandle
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
        if (Value.BinaryValue.size() > MAXDWORD)
        {
            ReportError(E_UNEXPECTED, L"Binary value is too long (name: " + Value.Name + L")");
            Result = E_UNEXPECTED;
            goto Cleanup;
        }
        Result = HRESULT_FROM_WIN32(RegSetValueExW(KeyHandle, Value.Name.c_str(), 0, Value.Type,
                Value.BinaryValue.data(), static_cast<DWORD>(Value.BinaryValue.size())));
        if (FAILED(Result))
        {
            ReportError(Result, L"Could not set value " + Value.Name + L" of key " + RegKey.Name);
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


// non-static function: documented in header.
_Must_inspect_result_
HRESULT InternalToHive
(
    _In_ const RegistryKey& RegKey,
    _In_ const std::wstring& OutputFilePath
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