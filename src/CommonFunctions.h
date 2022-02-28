// (C) Stormshield 2022
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#pragma once
#include <string>

/// @brief Delete .LOG1 and .LOG2 system files that were created when loading an application hive
/// @param[in] HiveFilePath Path to the hive file
void DeleteHiveLogFiles
(
    _In_ const std::wstring &HiveFilePath
);

/// @brief Report an error
/// @param[in] ErrorCode HRESULT value
/// @param[in] Context Optional context
void ReportError
(
    _In_ const HRESULT ErrorCode,
    _In_ const std::wstring& Context = std::wstring{}
);

/// @brief Perform global replacement of substring in a std::wstring
/// @param[in,out] String String on which to perform substitutions
/// @param[in] Pattern Substring that will be replaced by #Replacement in #String
/// @param[in] Replacement What is inserted in place of #Substring in #String
VOID GlobalStringSubstitute(
    _Inout_ std::wstring& String,
    _In_ const std::wstring& Pattern,
    _In_ const std::wstring& Replacement
);
