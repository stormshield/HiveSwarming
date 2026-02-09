// (C) Stormshield 2026
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "Constants.h"
#include "Conversions.h"
#include "CommonFunctions.h"
#include <sstream>
#include <iomanip>
#include <string_view>

/// <summary>
/// Render arbitrary data as hex string in a .reg file
/// </summary>
/// <param name="OutFileHandle">Handle to the output file, with write permissions</param>
/// <param name="FirstLineSizeSoFar">How many characters were already written on the current .reg file line, used for wrapping as standard .reg generation tools do</param>
/// <param name="RegValue">Value to render</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RenderBinaryValue
(
    _In_ HANDLE                OutFileHandle,
    _In_ size_t                FirstLineSizeSoFar,
    _In_ RegistryValue const & RegValue
)
{
    HRESULT Result = E_FAIL;
    size_t CurLineSizeSoFar = FirstLineSizeSoFar;
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
    BinaryRenditionStream << Constants::RegFiles::ValueTypeAndDataSeparator;

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
                BinaryRenditionStream << Constants::RegFiles::EscapedNewLine;
                for (size_t SpaceCount = 0; SpaceCount < Constants::RegFiles::HexNewLineLeadingSpaces; ++SpaceCount)
                {
                    BinaryRenditionStream << Constants::RegFiles::LeadingSpace;
                }
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

/// <summary>
/// Render REG_DWORD value in a .reg file
/// </summary>
/// <param name="OutFileHandle">Handle to the output file, with write permissions</param>
/// <param name="FirstLineSizeSoFar">How many characters were already written on the current .reg file line, used for wrapping as standard .reg generation tools do</param>
/// <param name="RegValue">Value to render</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RenderDwordValue
(
    _In_ HANDLE                OutFileHandle,
    _In_ size_t                FirstLineSizeSoFar,
    _In_ RegistryValue const & RegValue
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
    DwordRenditionStream << Constants::RegFiles::DwordPrefix << Constants::RegFiles::ValueTypeAndDataSeparator
                         << std::hex << std::setw(8) << std::setfill(L'0') << *(DWORD*)(RegValue.BinaryValue.data())
                         << Constants::RegFiles::NewLines;

    return WriteStringBufferToFile(OutFileHandle, DwordRenditionStream.str());

}

/// <summary>
/// Hiveswarming extension: Render REG_QWORD value in a .reg file
/// </summary>
/// <param name="OutFileHandle">Handle to the output file, with write permissions</param>
/// <param name="FirstLineSizeSoFar">How many characters were already written on the current .reg file line, used for wrapping as standard .reg generation tools do</param>
/// <param name="RegValue">Value to render</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RenderQwordValue
(
    _In_ HANDLE                OutFileHandle,
    _In_ size_t                FirstLineSizeSoFar,
    _In_ RegistryValue const & RegValue
)
{
    if (OutFileHandle == NULL || OutFileHandle == INVALID_HANDLE_VALUE)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_HANDLE;
    }

    if (RegValue.Type != REG_QWORD)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_INVALIDARG;
    }

    if (RegValue.BinaryValue.size() != sizeof(ULONGLONG))
    {
        return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }

    std::wostringstream QwordRenditionStream;
    QwordRenditionStream << Constants::RegFiles::QwordPrefix << Constants::RegFiles::ValueTypeAndDataSeparator
                         << std::hex << std::setw(16) << std::setfill(L'0') << *(ULONGLONG*)(RegValue.BinaryValue.data())
                         << Constants::RegFiles::NewLines;

    return WriteStringBufferToFile(OutFileHandle, QwordRenditionStream.str());

}

/// <summary>
/// Render REG_SZ value in a .reg file
/// </summary>
/// <param name="OutFileHandle">Handle to the output file, with write permissions</param>
/// <param name="FirstLineSizeSoFar">How many characters were already written on the current .reg file line, used for wrapping as standard .reg generation tools do</param>
/// <param name="RegValue">Value to render</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RenderStringValue
(
    _In_ HANDLE                OutFileHandle,
    _In_ size_t                FirstLineSizeSoFar,
    _In_ RegistryValue const & RegValue
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

/// <summary>
/// Hiveswarming extension: Render a value to .reg file as a zero-terminated string or sequence of zero-terminated strings
/// </summary>
/// <param name="OutFileHandle">Handle to the output file, with write permissions</param>
/// <param name="FirstLineSizeSoFar">How many characters were already written on the current .reg file line, used for wrapping as standard .reg generation tools do</param>
/// <param name="TypeSpecifier">What type to indicate</param>
/// <param name="RegValue">Value to render</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RenderMultiSzValue
(
    _In_ HANDLE                    OutFileHandle,
    _In_ size_t                    FirstLineSizeSoFar,
    _In_ std::wstring_view const & TypeSpecifier,
    _In_ RegistryValue     const & RegValue
)
{
    std::wstring WstringValue;
    if (OutFileHandle == NULL || OutFileHandle == INVALID_HANDLE_VALUE)
    {
        ReportError(E_HANDLE, L"Invalid parameter");
        return E_HANDLE;
    }

    if (RegValue.BinaryValue.size() % sizeof(WCHAR) != 0)
    {
        return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }

    WstringValue.assign((PWCHAR)RegValue.BinaryValue.data(), RegValue.BinaryValue.size() / sizeof(WCHAR));
    if (WstringValue.empty())
    {
        return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }

    if (WstringValue.empty() || WstringValue.back() != L'\0')
    {
        return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }

    size_t CurLineSizeSoFar = FirstLineSizeSoFar;
    std::wostringstream StringRenditionStream;
    StringRenditionStream << TypeSpecifier << Constants::RegFiles::ValueTypeAndDataSeparator;

    std::wstring_view Remainder { WstringValue };
    while (!Remainder.empty())
    {
        StringRenditionStream << L"\"";
        while (!Remainder.empty())
        {
            WCHAR c = Remainder.front();
            Remainder.remove_prefix(1);

            if (c == L'\0') {
                StringRenditionStream << L"\"";
                break;
            }

            switch (c) {
            case L'\n':
                StringRenditionStream << L'\r';
                break;

            case Constants::RegFiles::StringDelimiterEscape:
            case Constants::RegFiles::StringDelimiter:
                StringRenditionStream << Constants::RegFiles::StringDelimiterEscape;
                break;
            default:
                break;
            }

            StringRenditionStream << c;
        }

        if (Remainder.empty())
        {
            StringRenditionStream << Constants::RegFiles::NewLines;
            return WriteStringBufferToFile(OutFileHandle, StringRenditionStream.str());
        }
        else
        {
            StringRenditionStream << Constants::RegFiles::MultiSzSeparator;

            CurLineSizeSoFar += StringRenditionStream.view().length();
            if (CurLineSizeSoFar > Constants::RegFiles::MultiSzWrappingLimit - 2)
            {
                // adding ",\" would go over 80 characters, reg export typically breaks line here.
                StringRenditionStream << Constants::RegFiles::EscapedNewLine;

                HRESULT Result = WriteStringBufferToFile(OutFileHandle, StringRenditionStream.str());
                if (FAILED(Result)) {
                    return Result;
                }

                StringRenditionStream.str(L"");

                for (size_t SpaceCount = 0; SpaceCount < FirstLineSizeSoFar; ++SpaceCount)
                {
                    StringRenditionStream << Constants::RegFiles::LeadingSpace;
                }
                CurLineSizeSoFar = FirstLineSizeSoFar;
            }
        }
    }
    return WriteStringBufferToFile(OutFileHandle, StringRenditionStream.str());
}

/// <summary>
/// Render a registry value to a .reg file
/// </summary>
/// <param name="OutFileHandle">Handle to the output file, with write permissions</param>
/// <param name="RegValue">Value to render</param>
/// <param name="EnableExtensions">Whether to enable Hiveswarming extensions for REG_QWORD, REG_EXPAND_SZ and REG_MULTI_SZ values</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RenderRegistryValue
(
    _In_ HANDLE                OutFileHandle,
    _In_ RegistryValue const & RegValue,
    _In_ bool                  EnableExtensions
)
{
    HRESULT Result = E_FAIL;
    size_t FirstLineSizeSoFar = 0;
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
    else if (RegValue.Type == REG_SZ)
    {
        return RenderStringValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }
    else if (RegValue.Type == REG_QWORD && EnableExtensions)
    {
        return RenderQwordValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }
    else if (RegValue.Type == REG_MULTI_SZ && EnableExtensions)
    {
        return RenderMultiSzValue(OutFileHandle, FirstLineSizeSoFar, Constants::RegFiles::MultiSzPrefix, RegValue);
    }
    else if (RegValue.Type == REG_EXPAND_SZ && EnableExtensions)
    {
        return RenderMultiSzValue(OutFileHandle, FirstLineSizeSoFar, Constants::RegFiles::ExpandSzPrefix, RegValue);
    }
    else
    {
        return RenderBinaryValue(OutFileHandle, FirstLineSizeSoFar, RegValue);
    }
}

/// <summary>
/// Render a registry key and its child keys to a .reg file
/// </summary>
/// <param name="OutFileHandle">Handle to the output file, with write permissions</param>
/// <param name="RegKey">Key to render</param>
/// <param name="PathSoFar">Path to the parent key</param>
/// <param name="EnableExtensions">Whether to enable Hiveswarming extensions for REG_QWORD, REG_EXPAND_SZ and REG_MULTI_SZ values</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RenderRegistryKeyToRegFormat
(
    _In_ HANDLE              OutFileHandle,
    _In_ RegistryKey const & RegKey,
    _In_ std::wstring      & PathSoFar,
    _In_ bool                EnableExtensions
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
        Result = RenderRegistryValue(OutFileHandle, Value, EnableExtensions);
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
        Result = RenderRegistryKeyToRegFormat(OutFileHandle, Key, NewPath, EnableExtensions);
        if (FAILED(Result))
        {
            ReportError(E_HANDLE, L"Could not render registry key" + Key.Name);
            return Result;
        }
    }

    return S_OK;
}

_Must_inspect_result_ HRESULT InternalToRegfile
(
    _In_ RegistryKey  const & RegKey,
    _In_ std::wstring const & OutputFilePath,
    _In_ bool                 EnableExtensions
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

    {
        std::wstring EmptyPathForRootValue{};
        Result = RenderRegistryKeyToRegFormat(OutFileHandle, RegKey, EmptyPathForRootValue, EnableExtensions);
        if (FAILED(Result))
        {
            ReportError(Result, L"Could not render registry key");
            goto Cleanup;
        }
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