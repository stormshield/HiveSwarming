// (C) Stormshield 2026
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "CommonFunctions.h"
#include "Constants.h"
#include "Conversions.h"
#include <string_view>
#include <Windows.h>
#include <iterator>

/// <summary>
/// Read an entry in a .pol file and convert it to the internal representation of a registry key
/// </summary>
/// <param name="ReadHead">Byte sequence view on the .pol file</param>
/// <param name="Key">Registry key representation</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT ReadSinglePregEntry
(
    _Inout_ std::string_view & ReadHead,
    _Out_   RegistryKey      & Key
)
{
    HRESULT Result = E_FAIL;
    RegistryValue v;

    if (!ExpectAndConsumePODAtReadHead(ReadHead, Constants::PolFiles::EntryOpening))
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"Entry does not start with opening bracket");
        goto Cleanup;
    }

    {
        std::wstring_view ws = GetWstringViewFromStringView(ReadHead);
        const auto SemiColonPosition = ws.find(Constants::PolFiles::EntrySeparator);
        if (SemiColonPosition == ws.npos)
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"Key name separator not found");
            goto Cleanup;
        }

        const auto NullTerminatedKeyName = ws.substr(0, SemiColonPosition);
        if (NullTerminatedKeyName.empty() || NullTerminatedKeyName.back() != L'\0')
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"Key name not null-terminated");
            goto Cleanup;
        }

        Key.Name = NullTerminatedKeyName;
        Key.Name.pop_back();
        AdvanceReadHead(ReadHead, sizeof(WCHAR) * (SemiColonPosition + 1));
    }

    {
        std::wstring_view ws = GetWstringViewFromStringView(ReadHead);
        const auto SemiColonPosition = ws.find(Constants::PolFiles::EntrySeparator);
        if (SemiColonPosition == ws.npos)
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"Value name separator not found");
            goto Cleanup;
        }

        const auto NullTerminatedValueName = ws.substr(0, SemiColonPosition);
        if (NullTerminatedValueName.empty() || NullTerminatedValueName.back() != L'\0')
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"Value name not null-terminated");
            goto Cleanup;
        }

        v.Name = NullTerminatedValueName;
        v.Name.pop_back();
        AdvanceReadHead(ReadHead, sizeof(WCHAR) * (SemiColonPosition + 1));
    }

    if (!RetrieveAndConsumePODAtReadHead(ReadHead, v.Type))
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"Could not read value type " + v.Name);
        goto Cleanup;
    }

    if (!ExpectAndConsumePODAtReadHead(ReadHead, Constants::PolFiles::EntrySeparator))
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"Value type not followed by a semicolon");
        goto Cleanup;
    }

    DWORD ValueSize;
    if (!RetrieveAndConsumePODAtReadHead(ReadHead, ValueSize))
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"Could not read value size " + v.Name);
        goto Cleanup;
    }

    if (!ExpectAndConsumePODAtReadHead(ReadHead, Constants::PolFiles::EntrySeparator))
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"Value size not followed by a semicolon");
        goto Cleanup;
    }

    if (ValueSize == 0)
    {
        v.BinaryValue.clear();
    }
    else if (ReadHead.size() < ValueSize)
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"End of stream before end of value data");
        goto Cleanup;
    }
    else
    {
        v.BinaryValue.assign(ReadHead.begin(), ReadHead.begin() + ValueSize);
        AdvanceReadHead(ReadHead, ValueSize);
    }

    if (!ExpectAndConsumePODAtReadHead(ReadHead, Constants::PolFiles::EntryClosing))
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"Value data not followed by a closing bracket");
        goto Cleanup;
    }

    if (!v.Name.empty() || v.Type != 0)
    {
        Key.Values.emplace_back(std::move(v));
    }
    Result = S_OK;
Cleanup:

    return Result;
}


_Must_inspect_result_ HRESULT PolfileToInternal
(
    _In_  std::wstring      const & PolFilePath,
    _In_  std::wstring_view const & RootName,
    _Out_ RegistryKey             & RegKey
)
{
    // https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-gpreg/5c092c22-bf6b-4e7f-b180-b20743d368f5

    HRESULT Result = E_FAIL;
    std::string FileContents;
    std::vector<RegistryKey> KeysInFile;
    std::vector<RegistryKey> TempSubKeys;

    Result = ReadFileToString(PolFilePath, FileContents);
    if (FAILED(Result))
    {
        goto Cleanup;
    }

    {
        std::string_view ReadHead{ FileContents };

        if (!ExpectAndConsumeAtReadHead(ReadHead, Constants::PolFiles::Preamble))
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"File " + std::wstring{ PolFilePath } + L" PReg preamble not found");
            goto Cleanup;
        }

        if (!ExpectAndConsumePODAtReadHead(ReadHead, Constants::PolFiles::ExpectedVersion))
        {
            Result = E_UNEXPECTED;
            ReportError(Result, L"File " + std::wstring{ PolFilePath } + L" version not found");
            goto Cleanup;
        }

        while (!ReadHead.empty())
        {
            RegistryKey k;
            Result = ReadSinglePregEntry(ReadHead, k);
            if (FAILED(Result))
            {
                ReportError(Result, L"Reading .pol entries");
                goto Cleanup;
            }
            TempSubKeys.emplace_back(std::move(k));
        }
    }

    RegKey.Name = RootName;
    if (TempSubKeys.empty())
    {
        Result = S_OK;
        goto Cleanup;
    }

    // .pol files contain only a flat structure of registry values, each of which carries full key path.
    // Here we merge adjacent items where they are two values stored in the same key
    // However we do not reconstruct a full tree of keys.
    RegKey.Subkeys.emplace_back(std::move(TempSubKeys.front()));
    {
        auto It = TempSubKeys.begin();
        It++;
        for ( /* nothing */; It != TempSubKeys.end(); It++)
        {
            RegistryKey& Last = RegKey.Subkeys.back();
            if (It->Name == Last.Name)
            {
                std::move(It->Subkeys.begin(), It->Subkeys.end(), std::back_inserter(Last.Subkeys));
                It->Subkeys.clear();
                std::move(It->Values.begin(), It->Values.end(), std::back_inserter(Last.Values));
                It->Values.clear();
            }
            else
            {
                RegKey.Subkeys.emplace_back(std::move(*It));
            }
        }
    }

    Result = S_OK;

Cleanup:
    return Result;
}