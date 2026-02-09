// (C) Stormshield 2026
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#pragma once

#include <Windows.h>
#include <string>
#include <vector>

/// <summary>
/// Internal representation of a registry value
/// </summary>
struct RegistryValue {
    /// <summary>
    /// Name of the registry value. May contain the null character
    /// </summary>
    std::wstring Name;

    /// <summary>
    /// Type of the registry values, usually a small number.
    /// </summary>
    DWORD Type;

    /// <summary>
    /// Binary representation of the value
    /// </summary>
    std::vector<BYTE> BinaryValue;
};

/// <summary>
/// Internal representation of a registry key
/// </summary>
struct RegistryKey {
    /// <summary>
    /// Name of the registry key. May contain any character except for backslash.
    /// </summary>
    std::wstring Name;

    /// <summary>
    /// Container of child registry keys
    /// </summary>
    std::vector<RegistryKey> Subkeys;

    /// <summary>
    /// Container of registry values
    /// </summary>
    std::vector<RegistryValue> Values;
};

/// <summary>
/// Read a registry hive (binary) file, and convert it to the internal representation
/// </summary>
/// <param name="HiveFilePath">Path to the registry hive</param>
/// <param name="RootName">Path to the root key for export</param>
/// <param name="RegKey">Output internal structure</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT HiveToInternal
(
    _In_  std::wstring      const & HiveFilePath,
    _In_  std::wstring_view const & RootName,
    _Out_ RegistryKey             & RegKey
);

/// <summary>
/// Read a .reg (text) file, and convert it to the internal representation
/// </summary>
/// <param name="RegFilePath">Path to the registry .reg file</param>
/// <param name="RegKey">Output internal structure</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT RegfileToInternal
(
    _In_  std::wstring const & RegFilePath,
    _Out_ RegistryKey        & RegKey
);


/// <summary>
/// Read a .pol (binary) file, and convert it to the internal representation
/// </summary>
/// <param name="RegFilePath">Path to the policy (.pol) file</param>
/// <param name="RegKey">Output internal structure</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT PolfileToInternal
(
    _In_  std::wstring      const & FilePath,
    _In_  std::wstring_view const & RootName,
    _Out_ RegistryKey             & RegKey
);

/// <summary>
/// Create or overwrite a hive file from the internal representation of a registry key
/// </summary>
/// <param name="RegKey">Internal representation of the registry key</param>
/// <param name="OutputFilePath">Path of the desired hive file</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT InternalToHive
(
    _In_ RegistryKey  const & RegKey,
    _In_ std::wstring const & OutputFilePath
);

/// <summary>
/// Create or overwrite a .reg file from the internal representation of a registry key
/// </summary>
/// <param name="RegKey">Internal representation of the registry key</param>
/// <param name="OutputFilePath">Path of the desired .reg file</param>
/// <param name="EnableExtensions">Whether we should use the Hiveswarming .reg format extensions</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT InternalToRegfile
(
    _In_ RegistryKey  const & RegKey,
    _In_ std::wstring const & OutputFilePath,
    _In_ bool                 EnableExtensions
);

/// <summary>
/// Create or overwrite a .pol file from the internal representation of a registry key
/// </summary>
/// <param name="RegKey">Internal representation of the registry key</param>
/// <param name="OutputFilePath">Path of the desired .pol file</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT InternalToPolfile
(
    _In_ RegistryKey  const & RegKey,
    _In_ std::wstring const & OutputFilePath
);
