// (C) Stormshield 2020
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "Constants.h"
#include "Conversions.h"
#include "CommonFunctions.h"
#include <sstream>
#include <iomanip>
#include <string_view>

/// @brief Consume a list of registry values in a .reg file
/// @param[in,out] ValueList String view to the beginning of a list of values.
///                Updated to the next token after the list of values, newlines having been consumed.
/// @param[in] Values Container for the read values
/// @return HRESULT semantics
_Must_inspect_result_
static HRESULT ValueListToInternal
(
    _Inout_ std::wstring_view& ValueList,
    _Inout_ std::vector<RegistryValue>& Values
)
{
    auto ConsumeChars = [&ValueList](SIZE_T CharCount)
    {
        ValueList.remove_prefix(CharCount);
    };

    auto HasChar = [&ValueList](CONST WCHAR ExpectedChar) -> bool
    {
        bool ReturnValue = !ValueList.empty() && ValueList[0] == ExpectedChar;
        if (ReturnValue)
        {
            ValueList.remove_prefix(1);
        }
        return ReturnValue;
    };

    auto HasString = [&ValueList](const std::wstring& ExpectedString) -> bool
    {
        bool ReturnValue = ValueList.length() >= ExpectedString.length() && std::equal(ExpectedString.cbegin(), ExpectedString.cend(), ValueList.cbegin());
        if (ReturnValue)
        {
            ValueList.remove_prefix(ExpectedString.length());
        }
        return ReturnValue;
    };

    auto EndOfStream = [&ValueList]() -> bool { return ValueList.empty(); };

    do
    {
        if (ValueList.empty())
        {
            return S_OK;
        }

        // Values are all consumed when getting a new line.
        if (HasString(Constants::RegFiles::NewLines))
        {
            while (HasString(Constants::RegFiles::NewLines))
            {
                //
            }

            return S_OK;
        }


        RegistryValue Value;

        if (HasChar(Constants::RegFiles::DefaultValue))
        {
            Value.Name.clear();
        }
        else if (HasChar(L'"'))
        {
            // We must have a value name here
            size_t ClosingQuotePos = 0;
            std::wostringstream ValueNameStream;
            while (true)
            {
                if (ClosingQuotePos >= ValueList.length())
                {
                    ReportError(E_UNEXPECTED, L"Looking for closing quotation mark");
                    return E_UNEXPECTED;
                }
                if (ValueList[ClosingQuotePos] == L'"')
                {
                    break;
                }
                if (ClosingQuotePos + 1 >= ValueList.length())
                {
                    ReportError(E_UNEXPECTED, L"Buffer too short for value name");
                    return E_UNEXPECTED;
                }
                if (ValueList[ClosingQuotePos] == L'\\')
                {
                    ValueNameStream << ValueList[ClosingQuotePos + 1];
                    ClosingQuotePos += 2;
                }
                else if (ValueList[ClosingQuotePos] == L'\r' && ValueList[ClosingQuotePos + 1] == L'\n')
                {
                    // ignore \r before \n
                    ClosingQuotePos += 1;
                }
                else
                {
                    ValueNameStream << ValueList[ClosingQuotePos];
                    ClosingQuotePos += 1;
                }
            }
            Value.Name = ValueNameStream.str();
            ConsumeChars(ClosingQuotePos + 1);
        }
        else
        {
            ReportError(E_UNEXPECTED, L"Value name should be literal @ or begin with double quote");
            return E_UNEXPECTED;
        }

        if (EndOfStream())
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Value " << Value.Name << L" - Missing data";
            ReportError(E_UNEXPECTED, ErrorMessageStream.str());
            return E_UNEXPECTED;
        }
        if (!HasChar(Constants::RegFiles::ValueNameSeparator))
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Value name " << Value.Name << L" - Missing = sign";
            ReportError(E_UNEXPECTED, ErrorMessageStream.str());
            return E_UNEXPECTED;
        }

        if (EndOfStream())
        {
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Value name " << Value.Name << L" - No data after = sign";
            ReportError(E_UNEXPECTED, ErrorMessageStream.str());
            return E_UNEXPECTED;
        }

        if (HasString(Constants::RegFiles::DwordPrefix))
        {
            Value.Type = REG_DWORD;
            DWORD DwordValue;
            if (ValueList.length() <= 8)
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Value name " << Value.Name << L" - Buffer less than 8 characters after dword declaration";
                ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                return E_UNEXPECTED;
            }

            {
                std::wstring ReadValue{ &ValueList[0], 8 };
                std::wistringstream DwordReadingStream{ ReadValue, std::ios_base::in };
                DwordReadingStream >> std::hex >> std::setw(8) >> std::setfill(L'0') >> DwordValue;

                std::wostringstream DwordVerificationStream;
                DwordVerificationStream << std::hex << std::setw(8) << std::setfill(L'0') << DwordValue;

                std::wstring VerificationString{ DwordVerificationStream.str() };
                if (_wcsnicmp(VerificationString.c_str(), ReadValue.c_str(), 8) != 0)
                {
                    std::wostringstream ErrorMessageStream;
                    ErrorMessageStream << L"Value name " << Value.Name << L" - Could not parse dword from string " << ReadValue;
                    ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                    return E_UNEXPECTED;
                }
            }

            Value.BinaryValue.resize(sizeof(DWORD));
            CopyMemory(Value.BinaryValue.data(), &DwordValue, sizeof(DwordValue));

            ConsumeChars(8);

            if (!HasString(Constants::RegFiles::NewLines))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Value name " << Value.Name << L" - Dword value not followed by \\r\\n";
                ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                return E_UNEXPECTED;
            }

        }
        else if (HasString(Constants::RegFiles::HexPrefix))
        {
            // general case
            if (ValueList.empty())
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Value name " << Value.Name << L" - End of buffer after hex declaration";
                ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                return E_UNEXPECTED;
            }

            // in the general case, (xx) indicates the value type:
            //                        "myvalue"=hex(xx):...
            // for REG_BINARY, (xx) is omitted:
            //                        "myvalue"=hex:...
            // If we find the opening parenthese, we parse the registry type.
            if (HasChar(Constants::RegFiles::HexTypeSpecOpening))
            {
                size_t ClosingParenPos = ValueList.find_first_not_of(L"0123456789abcdefABCDEF");
                if (ClosingParenPos == ValueList.npos || ClosingParenPos == 0 || ValueList[ClosingParenPos] != Constants::RegFiles::HexTypeSpecClosing)
                {
                    std::wostringstream ErrorMessageStream;
                    ErrorMessageStream << L"Value name " << Value.Name << L" - Could not find closing parenthesis";
                    ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                    return E_UNEXPECTED;
                }

                {
                    std::wistringstream ValueTypeInStream{ std::wstring{ &ValueList[0], ClosingParenPos }, std::ios_base::in };
                    ValueTypeInStream >> std::hex >> Value.Type;
                }
                ConsumeChars(ClosingParenPos + 1);
            }
            else
            {
                Value.Type = REG_BINARY;
            }

            if (!HasChar(Constants::RegFiles::HexSuffix))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Value name " << Value.Name << L" - Missing : sign after hex declaration";
                ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                return E_UNEXPECTED;
            }

            while (true)
            {
                if (ValueList.empty())
                {
                    std::wostringstream ErrorMessageStream;
                    ErrorMessageStream << L"Value name " << Value.Name << L" - End of data while reading binary value";
                    ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                    return E_UNEXPECTED;
                }
                if (HasString(Constants::RegFiles::NewLines))
                {
                    // end of binary data
                    break;
                }
                else if (HasChar(Constants::RegFiles::HexByteSeparator))
                {
                    continue;
                }
                else if (HasString(Constants::RegFiles::HexByteNewLine))
                {
                    continue;
                }
                else if (ValueList.length() >= 2 && isxdigit(ValueList[0]) && isxdigit(ValueList[1]))
                {
                    {
                        int ByteVal;
                        std::wistringstream HexValueInStream{ std::wstring{ &ValueList[0], 2 }, std::ios_base::in };
                        HexValueInStream >> std::hex >> ByteVal;
                        Value.BinaryValue.push_back(static_cast<BYTE>(ByteVal));
                    }
                    ConsumeChars(2);
                    continue;
                }
                else
                {
                    std::wostringstream ErrorMessageStream;
                    ErrorMessageStream << L"Value name " << Value.Name << L" - Expecting two hexadecimal digits";
                    ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                    return E_UNEXPECTED;
                }
            }
        }
        else if (HasChar(L'"'))
        {
            Value.Type = REG_SZ;
            size_t ClosingQuotePos = 0;
            std::wostringstream ValueString;
            while (true)
            {
                if (ClosingQuotePos >= ValueList.length())
                {
                    std::wostringstream ErrorMessageStream;
                    ErrorMessageStream << L"Value name " << Value.Name << L" - Could not find end of string value";
                    ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                    return E_UNEXPECTED;
                }
                if (ValueList[ClosingQuotePos] == L'"')
                {
                    break;
                }
                if (ClosingQuotePos + 1 >= ValueList.length())
                {
                    std::wostringstream ErrorMessageStream;
                    ErrorMessageStream << L"Value name " << Value.Name << L" - Buffer too short, maybe missing newline after closing quotation mark";
                    ReportError(E_UNEXPECTED, ErrorMessageStream.str());
                    return E_UNEXPECTED;
                }
                if (ValueList[ClosingQuotePos] == L'\\')
                {
                    ValueString << ValueList[ClosingQuotePos + 1];
                    ClosingQuotePos += 2;
                }
                else if (ValueList[ClosingQuotePos] == L'\r' && ValueList[ClosingQuotePos + 1] == L'\n')
                {
                    // ignore \r before \n
                    ClosingQuotePos += 1;
                }
                else
                {
                    ValueString << ValueList[ClosingQuotePos];
                    ClosingQuotePos += 1;
                }
            }
            ConsumeChars(ClosingQuotePos + 1);

            // we must store a terminator in the registry value
            std::wstring SzVal = ValueString.str() + L'\0';
            Value.BinaryValue.resize(SzVal.length() * sizeof(WCHAR));
            CopyMemory(Value.BinaryValue.data(), SzVal.c_str(), Value.BinaryValue.size());

            if (!HasString(Constants::RegFiles::NewLines))
            {
                std::wostringstream ErrorMessageStream;
                ErrorMessageStream << L"Value name " << Value.Name << L" - Value not followed by new line";
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
_Must_inspect_result_
static HRESULT RegListToInternal
(
    _Inout_ std::wstring_view& RegList,
    _In_ const std::wstring& PathPrefix,
    _Inout_ std::vector<RegistryKey>& RegKeys
)
{
    HRESULT Result = E_FAIL;
    static const std::wstring KeyClosingAtEOL = Constants::RegFiles::KeyClosing + Constants::RegFiles::NewLines;

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
    } while (!RegList.empty());

    return S_OK;
}

// non-static function: documented in header.
_Must_inspect_result_
HRESULT RegfileToInternal
(
    _In_ const std::wstring& RegFilePath,
    _Out_ RegistryKey& RegKey
)
{
    HRESULT Result = E_FAIL;
    HANDLE InFileHandle = INVALID_HANDLE_VALUE;
    LARGE_INTEGER FileSize;
    std::wstring FileContents;
    std::vector<RegistryKey> KeysInFile;

    InFileHandle = CreateFileW(RegFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (InFileHandle == INVALID_HANDLE_VALUE)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Opening file " + RegFilePath);
        goto Cleanup;
    }

    if (!GetFileSizeEx(InFileHandle, &FileSize))
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Getting file size of " + RegFilePath);
        goto Cleanup;
    }

    if (FileSize.HighPart > 0)
    {
        Result = E_OUTOFMEMORY;
        ReportError(Result, L"File " + RegFilePath + L" is too large (larger than 4GB)");
        goto Cleanup;
    }

    if (FileSize.LowPart % sizeof(WCHAR) != 0)
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"File " + RegFilePath + L" should have an even size because it is expected to hold WCHAR code units only");
        goto Cleanup;
    }

    // resize buffer
    FileContents.resize(FileSize.LowPart / sizeof(WCHAR), L'\0');

    {
        DWORD BytesRead = 0;
        if (!ReadFile(InFileHandle, (PVOID)(&FileContents[0]), FileSize.LowPart, &BytesRead, NULL))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            goto Cleanup;
        }
        if (BytesRead != FileSize.LowPart)
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"File " + RegFilePath + L" could not be read to buffer");
            goto Cleanup;
        }
    }

    {
        std::wstring_view Remainder { FileContents };

        if (Remainder.length() < Constants::RegFiles::Preamble.length() || !std::equal(Constants::RegFiles::Preamble.cbegin(), Constants::RegFiles::Preamble.cend(), FileContents.cbegin()))
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"File " + RegFilePath + L" preamble not found");
            goto Cleanup;
        }

        Remainder.remove_prefix(Constants::RegFiles::Preamble.length());

        Result = RegListToInternal(Remainder, std::wstring{}, KeysInFile);
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
        if (Remainder.length() != 0)
        {
            Result = E_UNEXPECTED;
            std::wostringstream ErrorMessageStream;
            ErrorMessageStream << L"Conversion to internal structure left " << Remainder.length() << L" code units in the file unparsed";
            ReportError(Result, ErrorMessageStream.str());
            goto Cleanup;
        }

        RegKey = KeysInFile[0];
    }

    Result = S_OK;

Cleanup:
    if (InFileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(InFileHandle);
        InFileHandle = INVALID_HANDLE_VALUE;
    }

    return Result;
}
