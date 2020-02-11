// (C) Stormshield 2020
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "Constants.h"
#include "Conversions.h"
#include "CommonFunctions.h"
#include <sstream>
#include <iomanip>

/// @brief Append binary contents of a std::wstring to an opened file
/// @param[in] OutFileHandle Handle to the file
/// @param[in] String Input string
/// @note The string buffer is dumped to the file “as is”.
///       Null characters may be appended to the file if the string buffer includes some of them.
_Must_inspect_result_
static HRESULT WriteStringBufferToFile
(
    _In_ const HANDLE OutFileHandle,
    _In_ const std::wstring String
)
{
    HRESULT Result = E_FAIL;

    if (String.length() >= MAXDWORD / sizeof(decltype(String)::traits_type::char_type))
    {
        Result = HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        ReportError(Result, L"Writing data that is too large");
        return Result;
    }

    if (OutFileHandle == NULL || OutFileHandle == INVALID_HANDLE_VALUE)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_HANDLE;
    }

    {
        const DWORD BytesToWrite = static_cast<DWORD>(String.length() * sizeof(decltype(String)::traits_type::char_type));
        DWORD BytesWritten = 0;

        if (!WriteFile(OutFileHandle, (LPCVOID)String.c_str(), BytesToWrite, &BytesWritten, NULL))
        {
            Result = HRESULT_FROM_WIN32(GetLastError());
            ReportError(Result, L"Could not write to output file");
            return Result;
        }
        if (BytesWritten != BytesToWrite)
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"Bytes not fully written to file");
            return Result;
        }
    }
    return S_OK;
}

/// @brief Render the contents of a registry value in a .reg file
/// @param[in] OutFileHandle Handle to the file
/// @param[in] FirstLineSizeSoFar How many characters have already been written on the line when dumping
///                               the name of the value and the equal sign.
///                               This is used for mimicking .reg format line breaks before lines over 80 characters
/// @param[in] RegValue Representation of the registry value
/// @return HRESULT semantics
_Must_inspect_result_
static HRESULT RenderBinaryValue
(
    _In_ const HANDLE OutFileHandle,
    _In_ const SIZE_T FirstLineSizeSoFar,
    _In_ const RegistryValue& RegValue
)
{
    HRESULT Result = E_FAIL;
    SIZE_T CurLineSizeSoFar = FirstLineSizeSoFar;
    if (OutFileHandle == NULL || OutFileHandle == INVALID_HANDLE_VALUE)
    {
        return E_HANDLE;
    }

    std::wostringstream BinaryRenditionStream;
    BinaryRenditionStream << Constants::RegFiles::HexPrefix;
    if (RegValue.Type != REG_BINARY)
    {
        BinaryRenditionStream << Constants::RegFiles::HexTypeSpecOpening << std::hex << RegValue.Type << Constants::RegFiles::HexTypeSpecClosing;
    }
    BinaryRenditionStream << Constants::RegFiles::HexSuffix;

    CurLineSizeSoFar += BinaryRenditionStream.str().length();

    for (std::vector<BYTE>::const_iterator CurByteIt = RegValue.BinaryValue.cbegin(); CurByteIt != RegValue.BinaryValue.cend(); ++CurByteIt)
    {
        BinaryRenditionStream << std::hex << std::setfill(L'0') << std::setw(2) << *CurByteIt;
        CurLineSizeSoFar += 2;
        if (CurByteIt + 1 != RegValue.BinaryValue.cend())
        {
            BinaryRenditionStream << Constants::RegFiles::HexByteSeparator;
            CurLineSizeSoFar += 1;
            if (CurLineSizeSoFar > Constants::RegFiles::HexWrappingLimit - 4)
            {
                // adding "xx,\" would go over 80 characters, reg export typically breaks line here.
                BinaryRenditionStream << Constants::RegFiles::HexByteNewLine;
                CurLineSizeSoFar = Constants::RegFiles::HexNewLineLeadingSpaces;
            }
        }
    }

    BinaryRenditionStream << Constants::RegFiles::NewLines;

    Result = WriteStringBufferToFile(OutFileHandle, BinaryRenditionStream.str());
    if (FAILED(Result))
    {
        ReportError(Result, L"Could not write to output file");
        return Result;
    }

    return S_OK;
}

/// @brief Render the contents of a REG_DWORD registry value in a .reg file
/// @param[in] OutFileHandle Handle to the file
/// @param[in] FirstLineSizeSoFar Use for fallback to #RenderBinaryValue
/// @param[in] RegValue Representation of the registry value
/// @return HRESULT semantics
_Must_inspect_result_
static HRESULT RenderDwordValue
(
    _In_ const HANDLE OutFileHandle,
    _In_ const SIZE_T FirstLineSizeSoFar,
    _In_ const RegistryValue& RegValue
)
{
    if (OutFileHandle == NULL || OutFileHandle == INVALID_HANDLE_VALUE)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_HANDLE;
    }

    if (RegValue.Type != REG_DWORD)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_INVALIDARG;
    }

    if (RegValue.BinaryValue.size() != sizeof(DWORD))
    {
        return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }

    std::wostringstream DwordRenditionStream;
    DwordRenditionStream << L"dword:";
    DwordRenditionStream << std::hex << std::setw(8) << std::setfill(L'0') << *(DWORD*)(RegValue.BinaryValue.data());
    DwordRenditionStream << Constants::RegFiles::NewLines;

    return WriteStringBufferToFile(OutFileHandle, DwordRenditionStream.str());

}

/// @brief Render the contents of a REG_SZ registry value in a .reg file
/// @param[in] OutFileHandle Handle to the file
/// @param[in] FirstLineSizeSoFar Use for fallback to #RenderBinaryValue
/// @param[in] RegValue Representation of the registry value
/// @return HRESULT semantics
/// @note This falls back to binary rendition if the REG_SZ does not meet requirements such as:
///       - REG_SZ values should be terminated by a null character
///       - REG_SZ values may not contain other null characters
///       - REG_SZ value sizes should be a multiple of 2 as they store WCHAR-based values.
_Must_inspect_result_
static HRESULT RenderStringValue
(
    _In_ const HANDLE OutFileHandle,
    _In_ const SIZE_T FirstLineSizeSoFar,
    _In_ const RegistryValue& RegValue
)
{
    std::wstring WstringValue;
    if (OutFileHandle == NULL || OutFileHandle == INVALID_HANDLE_VALUE)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_HANDLE;
    }

    if (RegValue.Type != REG_SZ)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_INVALIDARG;
    }

    if (RegValue.BinaryValue.size() % sizeof(WCHAR) != 0)
    {
        return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }

    WstringValue.assign((PWCHAR) RegValue.BinaryValue.data(), RegValue.BinaryValue.size() / sizeof(WCHAR));
    if (WstringValue.empty())
    {
        return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }

    if (   WstringValue.empty()
        || WstringValue.back() != L'\0'
        || std::count(WstringValue.cbegin(), WstringValue.cend() - 1, L'\0') != 0
       )
    {
        return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }

    // Now we can render the value as REG_SZ: Remove null character
    WstringValue.pop_back();

    GlobalStringSubstitute(WstringValue, L"\\", L"\\\\");
    GlobalStringSubstitute(WstringValue, L"\"", L"\\\"");
    GlobalStringSubstitute(WstringValue, L"\n", L"\r\n");

    std::wostringstream StringRenditionStream;
    StringRenditionStream <<  L"\"" << WstringValue  << L"\"" << Constants::RegFiles::NewLines;
    return WriteStringBufferToFile(OutFileHandle, StringRenditionStream.str());
}

/// @brief Render a registry value and its contents in a .reg file
/// @param[in] OutFileHandle Handle to the file
/// @param[in] RegValue Representation of the registry value
/// @return HRESULT semantics
_Must_inspect_result_
HRESULT RenderRegistryValue
(
    _In_ const HANDLE OutFileHandle,
    _In_ const RegistryValue& RegValue
)
{
    HRESULT Result = E_FAIL;
    SIZE_T FirstLineSizeSoFar = 0;
    if (OutFileHandle == NULL || OutFileHandle == INVALID_HANDLE_VALUE)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_HANDLE;
    }

    std::wstring EscapedName = RegValue.Name;
    GlobalStringSubstitute(EscapedName, L"\\", L"\\\\");
    GlobalStringSubstitute(EscapedName, L"\"", L"\\\"");
    GlobalStringSubstitute(EscapedName, L"\n", L"\r\n");
    std::wostringstream Str;
    if (EscapedName.empty())
    {
        Str << Constants::RegFiles::DefaultValue << Constants::RegFiles::ValueNameSeparator;
    }
    else
    {
        Str << L"\"" << EscapedName << L"\"=";
    }
    Result = WriteStringBufferToFile(OutFileHandle, Str.str());
    if (FAILED(Result))
    {
        ReportError(Result, L"Could not write data to file");
        return Result;
    }

    FirstLineSizeSoFar += Str.str().length();

    if (RegValue.Type == REG_DWORD)
    {
        return RenderDwordValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }
    if (RegValue.Type == REG_SZ)
    {
        return RenderStringValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }
    return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
}

/// @brief Render a registry key and its values and subkeys in a .reg file
/// @param[in] OutFileHandle Handle to the file
/// @param[in] RegKey Representation of the registry key
/// @param[in] PathSoFar Path to this key
/// @return HRESULT semantics
_Must_inspect_result_
HRESULT RenderRegistryKey
(
    _In_ const HANDLE OutFileHandle,
    _In_ const RegistryKey& RegKey,
    _In_ const std::wstring &PathSoFar
)
{
    HRESULT Result = E_FAIL;
    if (OutFileHandle == NULL || OutFileHandle == INVALID_HANDLE_VALUE)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_HANDLE;
    }

    std::wstring NewPath;
    if (PathSoFar.empty())
    {
        NewPath = RegKey.Name;
    }
    else
    {
        NewPath = PathSoFar + Constants::RegFiles::PathSeparator + RegKey.Name;
    }

    std::wstring EscapedPath { NewPath };
    GlobalStringSubstitute(EscapedPath, L"\n", L"\r\n");

    {
        std::wostringstream KeySpecStream;
        KeySpecStream << Constants::RegFiles::KeyOpening << EscapedPath << Constants::RegFiles::KeyClosing << Constants::RegFiles::NewLines;

        Result = WriteStringBufferToFile(OutFileHandle, KeySpecStream.str());
        if (FAILED(Result))
        {
            ReportError(E_HANDLE, L"Could not write to output file");
            return Result;
        }
    }

    for (const RegistryValue &Value : RegKey.Values)
    {
        Result = RenderRegistryValue(OutFileHandle, Value);
        if (FAILED(Result))
        {
            ReportError(E_HANDLE, L"Could not render registry value" + Value.Name);
            return Result;
        }
    }

    {
        Result = WriteStringBufferToFile(OutFileHandle, Constants::RegFiles::NewLines );
        if (FAILED(Result))
        {
            ReportError(E_HANDLE, L"Could not write to output file");
            return Result;
        }
    }

    for (const RegistryKey &Key : RegKey.Subkeys)
    {
        Result = RenderRegistryKey(OutFileHandle, Key, NewPath);
        if (FAILED(Result))
        {
            ReportError(E_HANDLE, L"Could not render registry key" + Key.Name);
            return Result;
        }
    }

    return S_OK;
}

// non-static function: documented in header.
_Must_inspect_result_
HRESULT InternalToRegfile
(
    _In_ const RegistryKey& RegKey,
    _In_ const std::wstring& OutputFilePath
)
{
    HRESULT Result = E_FAIL;
    HANDLE OutFileHandle = INVALID_HANDLE_VALUE;

    OutFileHandle = CreateFileW(OutputFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (OutFileHandle == INVALID_HANDLE_VALUE)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Could not open file " + OutputFilePath + L" for writing");
        goto Cleanup;
    }

    {
        Result = WriteStringBufferToFile(OutFileHandle, Constants::RegFiles::Preamble);
        if (FAILED(Result))
        {
            ReportError(Result, L"Could not write to output file");
            goto Cleanup;
        }
    }

    Result = RenderRegistryKey(OutFileHandle, RegKey, std::wstring{});
    if (FAILED(Result))
    {
        ReportError(Result, L"Could not render registry key");
        goto Cleanup;
    }

    Result = S_OK;

Cleanup:
    if (OutFileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(OutFileHandle);
        OutFileHandle = INVALID_HANDLE_VALUE;
    }

    return Result;
}