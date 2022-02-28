// (C) Stormshield 2022
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include <windows.h>
#include "Constants.h"
#include "CommonFunctions.h"
#include <vector>
#include <iostream>
#include <iomanip>

void DeleteHiveLogFiles
(
    _In_ const std::wstring &HiveFilePath
)
{
    for (auto& Ext : Constants::Hives::LogFileExtensions)
    {
        std::wstring LogFilePath = HiveFilePath + Ext;
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

void ReportError
(
    _In_ const HRESULT ErrorCode,
    _In_ const std::wstring& Context
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

VOID GlobalStringSubstitute(
    _Inout_ std::wstring& String,
    _In_ const std::wstring& Pattern,
    _In_ const std::wstring& Replacement
)
{
    size_t Position = String.find(Pattern);
    while (Position != String.npos)
    {
        String.replace(Position, Pattern.length(), Replacement);
        Position = String.find(Pattern, Position + Replacement.length());
    }
}
