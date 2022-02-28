// (C) Stormshield 2022
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#pragma once

#include <Windows.h>
#include <string>
#include <vector>

/// Internal representation of a registry value.
struct RegistryValue {
    /// Name of the registry value. May not contain null character
    std::wstring Name;

    /// Type of the registry value. Usually a small number.
    DWORD Type;

    /// Binary representation of the underlying data as a byte buffer.
    std::vector<BYTE> BinaryValue;
};

/// Internal representation of a registry key.
struct RegistryKey {
    /// Name of the registry key. May contain any character except backslash
    std::wstring Name;

    /// Container of subkeys
    std::vector<RegistryKey> Subkeys;

    /// Container of values
    std::vector<RegistryValue> Values;
};

/// @brief Create an internal representation of a registry key from a registry hive (binary) file
/// @param[in] HiveFilePath Path to the registry hive
/// @param[in] RootName Path to the root key for export
/// @param[out] RegKey Internal structure
/// @return HRESULT semantics
/// @note For the moment, system files that are created (.LOG1, .LOG2) are not cleaned up.
_Must_inspect_result_
HRESULT HiveToInternal
(
    _In_ const std::wstring &HiveFilePath,
    _In_ const std::wstring &RootName,
    _Out_ RegistryKey& RegKey
);

/// @brief Create a .reg file from the internal representation of a registry key
/// @param[in] RegKey Representation of the registry key
/// @param[in] OutputFilePath Path of the desired output file
/// @return HRESULT semantics
/// @note #OutputFilePath is overwritten if it already exists
_Must_inspect_result_
HRESULT InternalToRegfile
(
    _In_ const RegistryKey& RegKey,
    _In_ const std::wstring &OutputFilePath
);

/// @brief Create a hive file from the internal representation of a registry key
/// @param[in] RegKey Representation of the registry key
/// @param[in] OutputFilePath Path of the desired output file
/// @return HRESULT semantics
/// @note #OutputFilePath is overwritten if it already exists
_Must_inspect_result_
HRESULT InternalToHive
(
    _In_ const RegistryKey& RegKey,
    _In_ const std::wstring &OutputFilePath
);

/// @brief Create an internal representation of a registry key from a registry .reg (text) file
/// @param[in] RegFilePath Path to the registry .reg file
/// @param[out] RegKey Internal structure
/// @return HRESULT semantics
_Must_inspect_result_
HRESULT RegfileToInternal
(
    _In_ const std::wstring& RegFilePath,
    _Out_ RegistryKey& RegKey
);