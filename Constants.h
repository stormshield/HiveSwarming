// (C) Stormshield 2022
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#pragma once
#include <windows.h>
#include <vector>
#include <string>

/// Program constants
namespace Constants {

    /// Command-line constants
    namespace Program {
        /// Switch for converting a hive to a .reg file
        static const std::wstring HiveToRegFileSwitch { L"--hive-to-reg-file" };

        /// Switch for converting a .reg file to a hive
        static const std::wstring RegFileToHiveSwitch { L"--reg-file-to-hive" };
    };

    /// Program defaults
    namespace Defaults {
        /// Default Root registry path in generated .reg files
        static const std::wstring ExportKeyPath { L"(HiveRoot)" };
    };

    /// Hive-specific constants
    namespace Hives {
        /// Extensions of extra files that are generated when manipulating registry hives
        static const std::vector<std::wstring> LogFileExtensions{ L".LOG1", L".LOG2" };

        /// Special value storing the destination of a symbolic link
        static const std::wstring SymbolicLinkValue { L"SymbolicLinkValue" };
    };

    /// .reg file-specific constants
    namespace RegFiles {
        /// New lines used in .reg files
        static const std::wstring NewLines { L"\r\n" };

        /// Preamble found in .reg files
        static const std::wstring Preamble { L"\ufeff" // Byte Order Mark
                                             L"Windows Registry Editor Version 5.00\r\n"
                                             L"\r\n" };

        /// Character used at start of line beginning a registry key path
        static const WCHAR KeyOpening { L'[' };

        /// Character delimiting parent key from its child in a registry path
        static const WCHAR PathSeparator { L'\\' };

        /// Character used at end of line closing a registry key path
        static const WCHAR KeyClosing { L']' };

        /// Character used for default registry values
        static const WCHAR DefaultValue { L'@' };

        /// Character used for separating value declaration from its content
        static const WCHAR ValueNameSeparator { L'=' };

        /// Sequence used for describing a DWORD rendition
        static const std::wstring DwordPrefix { L"dword:" };

        /// Sequence used for describing a hexadecimal rendition
        static const std::wstring HexPrefix { L"hex" };

        /// Character used when beginning the value type for a hexadecimal rendition
        static const WCHAR HexTypeSpecOpening { L'(' };

        /// Character used when ending the value type for a hexadecimal rendition
        static const WCHAR HexTypeSpecClosing { L')' };

        /// Character at the beginning of the hexadecimal rendition
        static const WCHAR HexSuffix { L':' };

        /// Byte separator in hexadecimal renditions
        static const WCHAR HexByteSeparator { L',' };

        /// Desired maximal line length when dumping hexadecimal rendition of registry values
        static const SIZE_T HexWrappingLimit = 80u;

        /// Count of leading spaces when wrapping a hexadecimal rendition
        static const SIZE_T HexNewLineLeadingSpaces = 2u;

        /// Space character at the beginning of a continuation line when wrapping hex values
        static const WCHAR LeadingSpace { L' '};

        /// Sequence used when continuing a hexadecimal rendition on a new line
        static const std::wstring HexByteNewLine { L"\\\r\n" };
    };
};

