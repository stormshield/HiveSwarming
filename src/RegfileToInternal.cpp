// (C) Stormshield 2026
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "Constants.h"
#include "Conversions.h"
#include "CommonFunctions.h"
#include <sstream>
#include <iomanip>
#include <string_view>
#include <Windows.h>

/// <summary>
/// Ways of rendering registry values
/// </summary>
enum class ValueRendering {
    /// <summary>
    /// Unspecified
    /// </summary>
    Unknown,

    /// <summary>
    /// Binary: hex:xx,xx,xx... or hex(tt):xx,xx,xx... for non REG_BINARY values
    /// </summary>
    Hexadecimal,

    /// <summary>
    /// REG_SZ: string between double quotes, with backslash as escape character for backslash or double quote
    /// </summary>
    String,

    /// <summary>
    /// dword:xxxxxxxx for DWORD values
    /// </summary>
    Dword,

    /// <summary>
    /// Hiveswarming extension: qword:xxxxxxxx for QWORD values
    /// </summary>
    Qword,

    /// <summary>
    /// Hiveswarming extension: multi_sz:"str1","str2",...,"" for REG_MULTI_SZ values (normally ended in an empty string)
    /// </summary>
    MultiSz,

    /// <summary>
    /// Hiveswarming extension: expand_sz:"str" for REG_EXPAND_SZ values
    /// </summary>
    ExpandSz,
};

/// <summary>
/// Read the name of a registry value in a .reg file.
/// </summary>
/// <param name="ReadHead">Unparsed portion of the .reg file. Updated upon success.</param>
/// <param name="Value">Registry value. On success, the function will fill Value.Name</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT ReadNameOfRegistryValue
(
    _Inout_ std::wstring_view & ReadHead,
    _Inout_ RegistryValue     & Value
)
{
    if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::DefaultValue))
    {
        Value.Name.clear();
    }
    else if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::StringDelimiter))
    {
        // We must have a value name here
        size_t ClosingQuotePos = 0;
        std::wostringstream ValueNameStream;
        while (true)
        {
            if (ClosingQuotePos >= ReadHead.length())
            {
                ReportError(E_UNEXPECTED, L"Looking for closing quotation mark");
                return E_UNEXPECTED;
            }
            if (ReadHead[ClosingQuotePos] == Constants::RegFiles::StringDelimiter)
            {
                break;
            }
            if (ClosingQuotePos + 1 >= ReadHead.length())
            {
                ReportError(E_UNEXPECTED, L"Buffer too short for value name");
                return E_UNEXPECTED;
            }
            if (ReadHead[ClosingQuotePos] == Constants::RegFiles::StringDelimiterEscape)
            {
                ValueNameStream << ReadHead[ClosingQuotePos + 1];
                ClosingQuotePos += 2;
            }
            else if (ReadHead[ClosingQuotePos] == L'\r' && ReadHead[ClosingQuotePos + 1] == L'\n')
            {
                // ignore \r before \n
                ClosingQuotePos += 1;
            }
            else
            {
                ValueNameStream << ReadHead[ClosingQuotePos];
                ClosingQuotePos += 1;
            }
        }
        Value.Name = ValueNameStream.view();
        AdvanceReadHead(ReadHead, ClosingQuotePos + 1);
    }
    else
    {
        ReportError(E_UNEXPECTED, L"Value name should be literal @ or begin with double quote");
        return E_UNEXPECTED;
    }
    return S_OK;
}

/// <summary>
/// Extract the registry value type found inside parentheses if any for hex-encoded values
/// </summary>
/// <param name="ReadHead">Unparsed portion of the .reg file. Updated upon success.</param>
/// <param name="Mode">Renderer of the value, used to determine default value in the absence of parentheses</param>
/// <param name="ValueType">Extracted registry type</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT ReadOptionalBinaryValueType
(
    _Inout_ std::wstring_view & ReadHead,
    _In_    ValueRendering      Mode,
    _Out_   DWORD             & ValueType
)
{
    if (ReadHead.empty())
    {
        ReportError(E_UNEXPECTED, L"End of buffer after hex declaration");
        return E_UNEXPECTED;
    }

    // in the general case, (xx) indicates the value type:
    //                        "myvalue"=hex(xx):...
    // for REG_BINARY, (xx) is omitted:
    //                        "myvalue"=hex:...
    // If we find the opening parenthesis, we parse the registry type.
    if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::HexTypeSpecOpening))
    {
        size_t ClosingParenPos = ReadHead.find_first_not_of(L"0123456789abcdefABCDEF");
        if (ClosingParenPos == ReadHead.npos || ClosingParenPos == 0 || ReadHead[ClosingParenPos] != Constants::RegFiles::HexTypeSpecClosing)
        {
            ReportError(E_UNEXPECTED, L"Could not find closing parenthesis");
            return E_UNEXPECTED;
        }

        {
            std::wistringstream ValueTypeInStream{ std::wstring{ &ReadHead[0], ClosingParenPos }, std::ios_base::in };
            ValueTypeInStream >> std::hex >> ValueType;
        }
        AdvanceReadHead(ReadHead, ClosingParenPos + 1);
    }
    else
    {
        switch (Mode)
        {
        case ValueRendering::Unknown:
            return E_UNEXPECTED;

        case ValueRendering::Dword:
            ValueType = REG_DWORD;
            break;

        case ValueRendering::Qword:
            ValueType = REG_QWORD;
            break;

        case ValueRendering::MultiSz:
            ValueType = REG_MULTI_SZ;
            break;

        case ValueRendering::ExpandSz:
            ValueType = REG_EXPAND_SZ;
            break;

        case ValueRendering::String:
            ValueType = REG_SZ;
            break;

        case ValueRendering::Hexadecimal:
        default:
            ValueType = REG_BINARY;
            break;
        }

    }
    return S_OK;
}

/// <summary>
/// Reads a hexadecimal numeric value of known size, for example for DWORD or QWORD values.
/// </summary>
/// <typeparam name="INTEGRAL_TYPE">Type of the value to read, determines size</typeparam>
/// <param name="ReadHead">Unparsed portion of the .reg file. Updated upon success.</param>
/// <param name="BinaryValue">Integral value as a byte sequence</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
template<std::regular INTEGRAL_TYPE> _Must_inspect_result_ static HRESULT ReadIntegralValue
(
    _Inout_ std::wstring_view & ReadHead,
    _Inout_ std::vector<BYTE> & BinaryValue
)
{
    static constexpr size_t HEXADECIMAL_REPRESENTATION_LENGTH = 2 * sizeof(INTEGRAL_TYPE);

    INTEGRAL_TYPE NumericValue{};

    if (ReadHead.length() <= HEXADECIMAL_REPRESENTATION_LENGTH)
    {
        std::wostringstream ErrorMessageStream;
        ErrorMessageStream << L"Buffer less than " << HEXADECIMAL_REPRESENTATION_LENGTH << " characters after declaration";
        ReportError(E_UNEXPECTED, ErrorMessageStream.str());
        return E_UNEXPECTED;
    }

    {
        std::wstring ReadValue{ &ReadHead[0], HEXADECIMAL_REPRESENTATION_LENGTH };
        std::wistringstream NumberReadingStream{ ReadValue, std::ios_base::in };
        NumberReadingStream >> std::hex >> NumericValue;

        std::wostringstream NumberVerificationStream;
        NumberVerificationStream << std::hex << std::setw(HEXADECIMAL_REPRESENTATION_LENGTH) << std::setfill(L'0') << NumericValue;

        std::wstring VerificationString{ NumberVerificationStream.str() };
        if (_wcsnicmp(VerificationString.c_str(), ReadValue.c_str(), HEXADECIMAL_REPRESENTATION_LENGTH) != 0)
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Could not parse number from string " << ReadValue;
            ReportError(E_UNEXPECTED, ErrorMessageStream.str());
            return E_UNEXPECTED;
        }
    }

    BinaryValue.assign(sizeof(INTEGRAL_TYPE), 0);
    CopyMemory(BinaryValue.data(), &NumericValue, sizeof(INTEGRAL_TYPE));

    AdvanceReadHead(ReadHead, HEXADECIMAL_REPRESENTATION_LENGTH);

    if (!ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::NewLines))
    {
        std::wostringstream ErrorMessageStream;
        ErrorMessageStream << L"Numeric value not followed by \\r\\n";
        ReportError(E_UNEXPECTED, ErrorMessageStream.str());
        return E_UNEXPECTED;
    }
    return S_OK;
}

/// <summary>
/// Convert back the hex representation of a value to the binary sequence. Consumes final newline.
/// </summary>
/// <param name="ReadHead">Unparsed portion of the .reg file. Updated upon success.</param>
/// <param name="BinaryValue">Output byte sequence</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
static HRESULT ReadHexadecimalData
(
    _Inout_ std::wstring_view & ReadHead,
    _Inout_ std::vector<BYTE> & BinaryValue
)
{
    BinaryValue.clear();
    while (true)
    {
        if (ReadHead.empty())
        {
            ReportError(E_UNEXPECTED, L"End of data while reading binary value");
            return E_UNEXPECTED;
        }
        if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::NewLines))
        {
            // end of binary data
            break;
        }
        else if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::HexByteSeparator))
        {
            continue;
        }
        else if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::EscapedNewLine))
        {
            while (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::LeadingSpace))
            {
            }
            continue;
        }
        else if (ReadHead.length() >= 2 && isxdigit(ReadHead[0]) && isxdigit(ReadHead[1]))
        {
            {
                // wcstoul could work if we had the guarantee of a null byte or non-xdigit byte after the number,
                // but in our case we consider it unsafe.
                int ByteVal;
                std::wistringstream HexValueInStream{ std::wstring{ &ReadHead[0], 2 }, std::ios_base::in };
                HexValueInStream >> std::hex >> ByteVal;
                BinaryValue.push_back(static_cast<BYTE>(ByteVal));
            }
            AdvanceReadHead(ReadHead, 2);
            continue;
        }
        else
        {
            ReportError(E_UNEXPECTED, L"Expecting two hexadecimal digits");
            return E_UNEXPECTED;
        }
    }
    return S_OK;
}

/// <summary>
/// Read a quoted string and transform it to the binary data as stored in registry, with UTF-16LE binary representation.
/// </summary>
/// <param name="ReadHead">Unparsed portion of the .reg file. Updated upon success.</param>
/// <param name="BinaryValue">Output byte sequence holding the de-quoted and unescaped string, and with the trailing L'\0' (two null bytes)</param>
/// <param name="AppendOutput">Append mode on the output parameter. If false, output is cleared first.</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT ReadString
(
    _Inout_ std::wstring_view & ReadHead,
    _Inout_ std::vector<BYTE> & BinaryValue,
    _In_    bool                AppendOutput
)
{
    if (!ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::StringDelimiter))
    {
        ReportError(E_UNEXPECTED, L"Double quote expected");
        return E_UNEXPECTED;
    }

    size_t ClosingQuotePos = 0;
    std::wostringstream ValueString;
    while (true)
    {
        if (ClosingQuotePos >= ReadHead.length())
        {
            ReportError(E_UNEXPECTED, L"Could not find end of string value");
            return E_UNEXPECTED;
        }
        if (ReadHead[ClosingQuotePos] == Constants::RegFiles::StringDelimiter)
        {
            break;
        }
        if (ClosingQuotePos + 1 >= ReadHead.length())
        {
            ReportError(E_UNEXPECTED, L"Buffer too short, maybe missing newline after closing quotation mark");
            return E_UNEXPECTED;
        }
        if (ReadHead[ClosingQuotePos] == Constants::RegFiles::StringDelimiterEscape)
        {
            ValueString << ReadHead[ClosingQuotePos + 1];
            ClosingQuotePos += 2;
        }
        else if (ReadHead[ClosingQuotePos] == L'\r' && ReadHead[ClosingQuotePos + 1] == L'\n')
        {
            // ignore the \r and skip to \n
            ClosingQuotePos += 1;
        }
        else
        {
            ValueString << ReadHead[ClosingQuotePos];
            ClosingQuotePos += 1;
        }
    }
    AdvanceReadHead(ReadHead, ClosingQuotePos + 1);

    std::wstring StrValue{ ValueString.view() };
    StrValue.push_back(L'\0');
    ValueString.str(L"");

    if (!AppendOutput) {
        BinaryValue.clear();
    }

    std::copy(
        (BYTE*)StrValue.data(),
        (BYTE*)StrValue.data() + (StrValue.length() * sizeof(WCHAR)),
        std::back_inserter(BinaryValue)
    );

    return S_OK;
}

/// <summary>
/// Hiveswarming extension: Read a multi-string text representation and transform it to the binary data as stored in registry.
/// </summary>
/// <param name="ReadHead">Unparsed portion of the .reg file. Updated upon success.</param>
/// <param name="BinaryValue">Output byte sequence that corresponds to how the multi-string is stored in registry</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT ReadMultiSzData
(
    _Inout_ std::wstring_view & ReadHead,
    _Inout_ std::vector<BYTE> & BinaryValue
)
{
    HRESULT Result = ReadString(ReadHead, BinaryValue, false);
    if (FAILED(Result))
    {
        ReportError(Result, L"String expected");
        return Result;
    }

    while (TRUE)
    {
        if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::NewLines))
        {
            // end of binary data
            break;
        }
        else if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::MultiSzSeparator))
        {
            while (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::EscapedNewLine))
            {
                while (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::LeadingSpace))
                {
                }
            }
        }
        Result = ReadString(ReadHead, BinaryValue, true);
        if (FAILED(Result))
        {
            ReportError(Result, L"String expected");
            return Result;
        }
    }

    return S_OK;
}

/// <summary>
/// Read the set of values under a registry keys and fill the internal representation of the key
/// </summary>
/// <param name="ReadHead">Unparsed portion of the .reg file, updated upon success</param>
/// <param name="Values">Internal representation for the set of registry values</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT ValueListToInternal
(
    _Inout_ std::wstring_view          & ReadHead,
    _Inout_ std::vector<RegistryValue> & Values
)
{
    do
    {
        if (ReadHead.empty())
        {
            return S_OK;
        }

        // Values are all consumed when getting a new line.
        if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::NewLines))
        {
            while (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::NewLines))
            {
            }
            return S_OK;
        }

        RegistryValue Value;

        {
            HRESULT Result = ReadNameOfRegistryValue(ReadHead, Value);
            if (FAILED(Result)) {
                ReportError(Result, L"Looking for value name");
                return Result;
            }
        }

        if (!ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::ValueNameSeparator))
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Value " << Value.Name << L" - Missing = sign";
            ReportError(E_UNEXPECTED, ErrorMessageStream.str());
            return E_UNEXPECTED;
        }

        ValueRendering Mode = ValueRendering::Unknown;
        if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::DwordPrefix))
        {
            Mode = ValueRendering::Dword;
        }
        else if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::QwordPrefix))
        {
            Mode = ValueRendering::Qword;
        }
        else if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::HexPrefix))
        {
            Mode = ValueRendering::Hexadecimal;
        }
        else if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::MultiSzPrefix))
        {
            Mode = ValueRendering::MultiSz;
        }
        else if (ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::ExpandSzPrefix))
        {
            Mode = ValueRendering::ExpandSz;
        }
        else
        {
            Mode = ValueRendering::String;
        }

        // string is just a string between double quotes. Other types use Typename[(regtype)]:encoded_value format
        if (Mode != ValueRendering::String)
        {
            HRESULT Result = ReadOptionalBinaryValueType(ReadHead, Mode, Value.Type);
            if (FAILED(Result))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Value " << Value.Name << L" - bad value";
                ReportError(Result, ErrorMessageStream.str());
                return Result;
            }

            if (!ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::ValueTypeAndDataSeparator))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Value " << Value.Name << L" - Missing : sign after type declaration";
                ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                return E_UNEXPECTED;
            }
        }
        else
        {
            Value.Type = REG_SZ;
        }

        HRESULT Result = E_FAIL;
        switch (Mode)
        {
        case ValueRendering::Dword:
            Result = ReadIntegralValue<DWORD>(ReadHead, Value.BinaryValue);
            break;

        case ValueRendering::Qword:
            Result = ReadIntegralValue<ULONGLONG>(ReadHead, Value.BinaryValue);
            break;

        case ValueRendering::Hexadecimal:
            Result = ReadHexadecimalData(ReadHead, Value.BinaryValue);
            break;

        case ValueRendering::MultiSz:
        case ValueRendering::ExpandSz:
            Result = ReadMultiSzData(ReadHead, Value.BinaryValue);
            break;

        case ValueRendering::String:
            Result = ReadString(ReadHead, Value.BinaryValue, false);
            break;
        }

        if (FAILED(Result))
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Value " << Value.Name << L" - Value not well-formatted";
            ReportError(Result, ErrorMessageStream.str());
            return Result;
        }

        if (Mode == ValueRendering::String)
        {
            if (!ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::NewLines))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Value " << Value.Name << L" - Value not followed by new line";
                ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                return E_UNEXPECTED;
            }
        }

        Values.emplace_back(std::move(Value));
    } while (TRUE);
}

/// @brief Serializes the keys from a .reg file to internal representation
/// @param[in,out] RegList Remainder of the .reg file before and after serialization
///                        Updated to the next token after all keys (and their values) that
///                        begin with #PathPrefix, newlines having been consumed.
/// @param[in] PathPrefix Prefix to all keys that are to be read
/// @param[in,out] RegKeys Container for all read keys

/// <summary>
/// Consume all sub-keys of a given key and their values, and converts to internal representation. Stops at first key not under given path prefix.
/// </summary>
/// <param name="RegList">Unparsed portion of the .reg file, updated upon success</param>
/// <param name="PathPrefix">Path of the parent key, used for detecting child keys. Empty string for root key.</param>
/// <param name="RegKeys">Internal representation of all found sub-keys</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RegListToInternal
(
    _Inout_ std::wstring_view        & RegList,
    _In_    std::wstring const       & PathPrefix,
    _Inout_ std::vector<RegistryKey> & RegKeys
)
{
    HRESULT Result = E_FAIL;
    static const std::wstring KeyClosingAtEOL = std::wstring{ Constants::RegFiles::KeyClosing } + std::wstring{ Constants::RegFiles::NewLines };

    // remove additional line breaks
    while (RegList.length() >= 2 && RegList[0] == L'\r' && RegList[1] == L'\n')
    {
        RegList.remove_prefix(2);
    }

    if (RegList.empty())
    {
        std::wostringstream ErrorMessageStream;
        ErrorMessageStream << L"Reading key beginning with prefix " << (PathPrefix.empty() ? L"<empty>" : PathPrefix.c_str()) << L" - Expecting content";
        ReportError(E_UNEXPECTED, ErrorMessageStream.str());
        return E_UNEXPECTED;
    }

    do
    {
        if (RegList[0] != Constants::RegFiles::KeyOpening)
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Reading key beginning with prefix " << (PathPrefix.empty() ? L"<empty>" : PathPrefix.c_str()) << L" - Line does not begin with opening bracket";
            ReportError(E_UNEXPECTED, ErrorMessageStream.str());
            return E_UNEXPECTED;
        }
        auto EndKeyPos = RegList.find(KeyClosingAtEOL, 1);
        if (EndKeyPos == RegList.npos)
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Reading key beginning with prefix " << (PathPrefix.empty() ? L"<empty>" : PathPrefix.c_str()) << L" - Could not find closing bracket followed by new line";
            ReportError(E_UNEXPECTED, ErrorMessageStream.str());
            return E_UNEXPECTED;
        }

        const std::wstring_view KeyPath{ &RegList[1], EndKeyPos - 1 };

        if (KeyPath.length() <= PathPrefix.length() || !std::equal(PathPrefix.cbegin(), PathPrefix.cend(), KeyPath.cbegin()))
        {
            // not a sub-key for our caller. maybe a parent caller could handle this.
            break;
        }
        const std::wstring_view KeyName{ &KeyPath[PathPrefix.length()], KeyPath.length() - PathPrefix.length() };

        RegistryKey NewKey;
        NewKey.Name = KeyName;
        // keys may have newlines in their name
        GlobalStringSubstitute(NewKey.Name, L"\r\n", L"\n");

        RegList.remove_prefix(EndKeyPos + 1);
        RegList.remove_prefix(Constants::RegFiles::NewLines.length());
        Result = ValueListToInternal(RegList, NewKey.Values);
        if (FAILED(Result))
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Reading key " << PathPrefix << KeyName << L" - Could not read values";
            ReportError(E_UNEXPECTED, ErrorMessageStream.str());
            return Result;
        }

        if (!RegList.empty())
        {
            std::wstring NewPrefix = std::wstring{ KeyPath } + Constants::RegFiles::PathSeparator;
            Result = RegListToInternal(RegList, NewPrefix, NewKey.Subkeys);
            if (FAILED(Result))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Checking if there are keys beginning with " << NewPrefix;
                ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                return Result;
            }
        }

        RegKeys.emplace_back(std::move(NewKey));

        while (RegList.length() >= 2 && RegList[0] == L'\r' && RegList[1] == L'\n')
        {
            RegList.remove_prefix(1);
        }
    } while (!RegList.empty());

    return S_OK;
}

// non-static function: documented in header.
_Must_inspect_result_ HRESULT RegfileToInternal
(
    _In_  std::wstring const & RegFilePath,
    _Out_ RegistryKey        & RegKey
)
{
    HRESULT Result = E_FAIL;
    std::wstring FileContents;
    std::vector<RegistryKey> KeysInFile;

    Result = ReadFileToWString(RegFilePath, FileContents);
    if (FAILED(Result))
    {
        goto Cleanup;
    }

    {
        std::wstring_view ReadHead { FileContents };

        if (!ExpectAndConsumeAtReadHead(ReadHead, Constants::RegFiles::Preamble))
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"File " + RegFilePath + L" preamble not found");
            goto Cleanup;
        }

        Result = RegListToInternal(ReadHead, std::wstring{}, KeysInFile);
        if (FAILED(Result))
        {
            goto Cleanup;
        }

        // at first level, should have exactly one registry key
        if (KeysInFile.size() != 1)
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"Multiple root keys were found in the registry file");
            goto Cleanup;
        }

        // whole file should have been slurped.
        if (ReadHead.length() != 0)
        {
            Result = E_UNEXPECTED;
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Conversion to internal structure left " << ReadHead.length() << L" code units in the file unparsed";
            ReportError(Result, ErrorMessageStream.str());
            goto Cleanup;
        }

        std::swap(RegKey, KeysInFile[0]);
    }

    Result = S_OK;

Cleanup:

    return Result;
}
