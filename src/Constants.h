// (C) Stormshield 2026
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#pragma once
#include <windows.h>
#include <string_view>
#include <array>

/// <summary>
/// Program constants
/// </summary>
namespace Constants {

    /// <summary>
    /// Command-line constants
    /// </summary>
    namespace Program {

        /// <summary>
        /// Command-line switch, used for specifying input format
        /// </summary>
        static constexpr std::wstring_view FromSwitch{ L"--from" };

        /// <summary>
        /// Command-line switch, used for specifying output format
        /// </summary>
        static constexpr std::wstring_view ToSwitch{ L"--to" };

        /// <summary>
        /// Command-line value for input/output formats: registry hive
        /// </summary>
        static constexpr std::wstring_view FormatHive{ L"hive" };

        /// <summary>
        /// Command-line value for input/output formats: .reg file
        /// </summary>
        static constexpr std::wstring_view FormatReg{ L"reg" };

        /// <summary>
        /// Command-line value for input/output formats: .reg file with Hiveswarming extensions
        /// </summary>
        static constexpr std::wstring_view FormatRegWithHiveswarmingExtensions{ L"reg+" };

        /// <summary>
        /// Command-line value for input/output formats: .pol file
        /// </summary>
        static constexpr std::wstring_view FormatPol{ L"pol" };
    };

    /// <summary>
    /// Program defaults
    /// </summary>
    namespace Defaults {
        /// <summary>
        /// Default root key path when exporting to .reg
        /// </summary>
        static constexpr std::wstring_view ExportKeyPath { L"(HiveRoot)" };
    };

    /// <summary>
    /// Hive-specific constants
    /// </summary>
    namespace Hives {
        /// <summary>
        /// Extensions of extra files that are generated when manipulating registry hives
        /// </summary>
        static constexpr std::array<std::wstring_view, 2> LogFileExtensions{ L".LOG1", L".LOG2" };

        /// <summary>
        /// Value name indicating the destination of a symbolic link for keys with symbolic link flag
        /// </summary>
        static constexpr std::wstring_view SymbolicLinkValue { L"SymbolicLinkValue" };
    };

    /// <summary>
    /// .reg format-specific constants
    /// </summary>
    namespace RegFiles {
        /// <summary>
        /// Newline sequence used in .reg files
        /// </summary>
        static constexpr std::wstring_view NewLines { L"\r\n" };

        /// <summary>
        /// Preamble of .reg files
        /// </summary>
        static constexpr std::wstring_view Preamble { L"\ufeff" // Byte Order Mark
                                             L"Windows Registry Editor Version 5.00\r\n"
                                             L"\r\n" };

        /// <summary>
        /// Character expected before the path to a registry key
        /// </summary>
        static constexpr WCHAR KeyOpening { L'[' };

        /// <summary>
        /// Component separator in registry paths
        /// </summary>
        static constexpr WCHAR PathSeparator { L'\\' };

        /// <summary>
        /// Character expected after the path to a registry key
        /// </summary>
        static constexpr WCHAR KeyClosing { L']' };

        /// <summary>
        /// String used in place of a quoted value name, for the default value
        /// </summary>
        static constexpr std::wstring_view DefaultValue{ L"@" };

        /// <summary>
        /// Character delimiting value names and string values
        /// </summary>
        static constexpr WCHAR StringDelimiter { L'"' };

        /// <summary>
        /// Character used to escape the delimiter in delimited strings
        /// </summary>
        static constexpr WCHAR StringDelimiterEscape { L'\\' };

        /// <summary>
        /// Character used to separate the name of a registry value and its contents
        /// </summary>
        static constexpr WCHAR ValueNameSeparator { L'=' };

        /// <summary>
        /// Type specifier for DWORD values
        /// </summary>
        static constexpr std::wstring_view DwordPrefix { L"dword" };

        /// <summary>
        /// Hiveswarming extension: Type specifier for QWORD values
        /// </summary>
        static constexpr std::wstring_view QwordPrefix { L"qword" };

        /// <summary>
        /// Specifier for Hex-encoded values
        /// </summary>
        static constexpr std::wstring_view HexPrefix{ L"hex" };

        /// <summary>
        /// Hiveswarming extension: Type specifier for string list
        /// </summary>
        static constexpr std::wstring_view MultiSzPrefix{ L"multi_sz" };

        /// <summary>
        /// Hiveswarming extension: Type specifier for REG_EXPAND_SZ
        /// </summary>
        static constexpr std::wstring_view ExpandSzPrefix{ L"expand_sz" };

        /// <summary>
        /// For hex-encoded values, character that introduces numeric registry value type
        /// </summary>
        static constexpr WCHAR HexTypeSpecOpening { L'(' };

        /// <summary>
        /// For hex-encoded values, character expected after numeric registry value type
        /// </summary>
        static constexpr WCHAR HexTypeSpecClosing { L')' };

        /// <summary>
        /// For hex-encoded values, character expected before hexadecimal sequence
        /// </summary>
        static constexpr WCHAR ValueTypeAndDataSeparator { L':' };

        /// <summary>
        /// Hiveswarming extension: For string list values, character expected between strings
        /// </summary>
        static constexpr WCHAR MultiSzSeparator { L',' };

        /// <summary>
        /// For hex-encoded values, byte separator
        /// </summary>
        static constexpr WCHAR HexByteSeparator { L',' };

        /// <summary>
        /// For hexadecimal rendition of values, line length after which new bytes should be put to a new line
        /// </summary>
        static constexpr size_t HexWrappingLimit = 80u;

        /// <summary>
        /// Hiveswarming extension: for string list values, line length after which new strings should be put to a new line
        /// </summary>
        static constexpr size_t MultiSzWrappingLimit = 80u;

        /// <summary>
        /// Number of leading spaces on continuation lines for wrapped hex-encoded values
        /// </summary>
        static constexpr size_t HexNewLineLeadingSpaces = 2u;

        /// <summary>
        /// Hiveswarming extension: number of leading spaces on continuation lines for wrapped string list values
        /// </summary>
        static constexpr size_t MultiSzMultiLineLeadingSpaces = 8u;

        /// <summary>
        /// Character used for leading spaces in continuation lines
        /// </summary>
        static constexpr WCHAR LeadingSpace { L' '};

        /// <summary>
        /// Character sequence used to indicate that a value rendition is continued on the next line
        /// </summary>
        static constexpr std::wstring_view EscapedNewLine { L"\\\r\n" };
    };

    /// <summary>
    /// .pol format-specific constants.
    /// See https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-gpreg/5c092c22-bf6b-4e7f-b180-b20743d368f5
    /// </summary>
    namespace PolFiles {
        /// <summary>
        /// Magic number found in .pol file header
        /// </summary>
        static constexpr std::string_view Preamble{ "PReg" };

        /// <summary>
        /// Version number for .pol file format
        /// </summary>
        static constexpr DWORD ExpectedVersion = 0x00000001;

        /// <summary>
        /// Character indicating the start of an entry
        /// </summary>
        static constexpr WCHAR EntryOpening{ L'[' };

        /// <summary>
        /// Character separating entry elements
        /// </summary>
        static constexpr WCHAR EntrySeparator{ L';' };

        /// <summary>
        /// Character indicating the end of an entry
        /// </summary>
        static constexpr WCHAR EntryClosing{ L']' };
    };
};

