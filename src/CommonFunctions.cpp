// (C) Stormshield 2026
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include <windows.h>
#include "Constants.h"
#include "CommonFunctions.h"
#include <vector>
#include <iostream>
#include <iomanip>

void ReportError
(
    _In_ HRESULT                   ErrorCode,
    _In_ std::wstring_view const & Context
)
{
    LPWSTR MessageBuffer = NULL;
    DWORD FmtResult = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                     FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
                                     NULL, ErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                     (LPWSTR)&MessageBuffer, 0, NULL);

    if (!Context.empty())
    {
        std::wcerr << Context << L":" << std::endl;
    }

    if (FmtResult == 0)
    {
        std::wcerr << L"ERROR " << std::hex << std::setw(8) << ErrorCode << L" (could not format message)";
    }
    else
    {
        std::wcerr << L"ERROR " << std::hex << std::setw(8) << ErrorCode << L" (" << MessageBuffer << L")";
        LocalFree((PVOID)MessageBuffer);
        MessageBuffer = NULL;
    }

    std::wcerr << std::endl << std::endl;
}

#pragma region File ⟷ in-memory objects conversions

_Must_inspect_result_ HRESULT ReadFileToString
(
    _In_  std::wstring const & FilePath,
    _Out_ std::string        & FileContents
)
{
    HRESULT Result = E_FAIL;
    HANDLE InFileHandle = INVALID_HANDLE_VALUE;
    LARGE_INTEGER FileSize;

    InFileHandle = CreateFileW(FilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (InFileHandle == INVALID_HANDLE_VALUE)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Opening file " + std::wstring{ FilePath });
        goto Cleanup;
    }

    if (!GetFileSizeEx(InFileHandle, &FileSize))
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Getting file size of " + std::wstring{ FilePath });
        goto Cleanup;
    }

    if (FileSize.HighPart > 0)
    {
        Result = E_OUTOFMEMORY;
        ReportError(Result, L"File " + std::wstring{ FilePath } + L" is too large (larger than 4GB)");
        goto Cleanup;
    }

    FileContents.resize(FileSize.LowPart, '\0');

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
            ReportError(Result, L"File " + std::wstring{ FilePath } + L" could not be read to buffer");
            goto Cleanup;
        }
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

_Must_inspect_result_ HRESULT ReadFileToWString
(
    _In_  std::wstring const & FilePath,
    _Out_ std::wstring       & FileContents
)
{
    HRESULT Result = E_FAIL;
    HANDLE InFileHandle = INVALID_HANDLE_VALUE;
    LARGE_INTEGER FileSize;

    InFileHandle = CreateFileW(FilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (InFileHandle == INVALID_HANDLE_VALUE)
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Opening file " + FilePath);
        goto Cleanup;
    }

    if (!GetFileSizeEx(InFileHandle, &FileSize))
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Getting file size of " + FilePath);
        goto Cleanup;
    }

    if (FileSize.HighPart > 0)
    {
        Result = E_OUTOFMEMORY;
        ReportError(Result, L"File " + FilePath + L" is too large (larger than 4GB)");
        goto Cleanup;
    }

    if (FileSize.LowPart % sizeof(WCHAR) != 0)
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"File " + FilePath + L" should have an even size because it is expected to hold WCHAR code units only");
        goto Cleanup;
    }

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
            ReportError(Result, L"File " + FilePath + L" could not be read to buffer");
            goto Cleanup;
        }
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

_Must_inspect_result_ HRESULT WriteStringBufferToFile
(
    _In_ HANDLE                   OutFileHandle,
    _In_ std::string_view const & String
)
{
    HRESULT Result = E_FAIL;

    if (String.length() >= MAXDWORD)
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
        const DWORD BytesToWrite = static_cast<DWORD>(String.length());
        DWORD BytesWritten = 0;

        if (!WriteFile(OutFileHandle, (LPCVOID)String.data(), BytesToWrite, &BytesWritten, NULL))
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

_Must_inspect_result_ HRESULT WriteStringBufferToFile
(
    _In_ HANDLE                    OutFileHandle,
    _In_ std::wstring_view const & String
)
{
    using CHAR_TYPE = std::remove_reference_t<decltype(String)>::traits_type::char_type;
    HRESULT Result = E_FAIL;

    if (String.length() >= MAXDWORD / sizeof(CHAR_TYPE))
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
        const DWORD BytesToWrite = static_cast<DWORD>(String.length() * sizeof(CHAR_TYPE));
        DWORD BytesWritten = 0;

        if (!WriteFile(OutFileHandle, (LPCVOID)String.data(), BytesToWrite, &BytesWritten, NULL))
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

#pragma endregion

#pragma region In-memory sequences helpers

std::wstring_view GetWstringViewFromStringView
(
    _In_ std::string_view const & v
) noexcept
{
    auto const new_size = v.size() / sizeof(WCHAR);
    return std::wstring_view(reinterpret_cast<PCWCHAR>(v.data()), new_size);
}

#pragma endregion

void DeleteHiveLogFiles
(
    _In_ std::wstring_view const & HiveFilePath
)
{
    for (std::wstring_view const& Ext : Constants::Hives::LogFileExtensions)
    {
        std::wstring LogFilePath = std::wstring{ HiveFilePath } + std::wstring{ Ext };
        DWORD Attributes = GetFileAttributesW(LogFilePath.c_str());
        if (Attributes != INVALID_FILE_ATTRIBUTES)
        {
            Attributes &= ~FILE_ATTRIBUTE_HIDDEN;
            Attributes &= ~FILE_ATTRIBUTE_SYSTEM;
            SetFileAttributesW(LogFilePath.c_str(), Attributes);
            DeleteFileW(LogFilePath.c_str());
        }
    }
}

void GlobalStringSubstitute
(
    _Inout_ std::wstring       & String,
    _In_    std::wstring const & Pattern,
    _In_    std::wstring const & Replacement
)
{
    if (Pattern.empty()) return;
    size_t Position = String.find(Pattern);
    while (Position != String.npos)
    {
        String.replace(Position, Pattern.length(), Replacement);
        Position = String.find(Pattern, Position + Replacement.length());
    }
}

