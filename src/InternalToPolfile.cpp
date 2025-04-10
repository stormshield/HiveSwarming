// (C) Stormshield 2025
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include "Constants.h"
#include "Conversions.h"
#include "CommonFunctions.h"
#include <sstream>
#include <iomanip>
#include <string_view>

/// <summary>
/// Writes the internal representation of a registry value to a .pol file
/// </summary>
/// <param name="OutFileHandle">Handle to the output file, with write access</param>
/// <param name="KeyPath">Full path to the registry key</param>
/// <param name="Value">Internal representation of the registry value</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RenderRegistryValueToPolFormat
(
    _In_ HANDLE                OutFileHandle,
    _In_ std::wstring  const & KeyPath,
    _In_ RegistryValue const & Value
)
{
    HRESULT Result = E_FAIL;

    std::wstring NullTerminatedKeyPath = KeyPath;
    NullTerminatedKeyPath.push_back(L'\0');

    std::wstring NullTerminatedValueName = Value.Name;
    NullTerminatedValueName.push_back(L'\0');

    DWORD ValueSize = 0;
    DWORD BytesWritten = 0;

    if (Value.BinaryValue.size() > MAXDWORD)
    {
        Result = E_UNEXPECTED;
        ReportError(Result, L"Value too long");
        return Result;
    }
    ValueSize = static_cast<DWORD>(Value.BinaryValue.size());

    Result = WritePODToFile(OutFileHandle, Constants::PolFiles::EntryOpening);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write entry opening bracket");
        return Result;
    }

    Result = WriteStringBufferToFile(OutFileHandle, NullTerminatedKeyPath);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write key name");
        return Result;
    }

    Result = WritePODToFile(OutFileHandle, Constants::PolFiles::EntrySeparator);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write separator between key name and value name");
        return Result;
    }

    Result = WriteStringBufferToFile(OutFileHandle, NullTerminatedValueName);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write value name");
        return Result;
    }

    Result = WritePODToFile(OutFileHandle, Constants::PolFiles::EntrySeparator);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write separator between value name and value type");
        return Result;
    }

    Result = WritePODToFile(OutFileHandle, Value.Type);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write value type");
        return Result;
    }

    Result = WritePODToFile(OutFileHandle, Constants::PolFiles::EntrySeparator);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write separator between value type and value size");
        return Result;
    }

    Result = WritePODToFile(OutFileHandle, ValueSize);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write value size");
        return Result;
    }

    Result = WritePODToFile(OutFileHandle, Constants::PolFiles::EntrySeparator);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write separator between value size and value data");
        return Result;
    }

    if (!WriteFile(OutFileHandle, (LPCVOID)Value.BinaryValue.data(), ValueSize, &BytesWritten, NULL))
    {
        Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Could not write value data to .pol file");
        return Result;
    }

    if (BytesWritten != ValueSize)
    {
        HRESULT Result = E_UNEXPECTED;
        ReportError(Result, L"Bytes not fully written to file");
        return Result;
    }

    Result = WritePODToFile(OutFileHandle, Constants::PolFiles::EntryClosing);
    if (FAILED(Result))
    {
        ReportError(E_HANDLE, L"Could not write entry closing bracket");
        return Result;
    }

    return S_OK;
}

/// <summary>
/// Writes the internal representation of all registry values pertaining to a key, to a .pol file
/// </summary>
/// <param name="OutFileHandle">Handle to the output file, with write access</param>
/// <param name="RegistryKey">Internal representation of the registry key</param>
/// <param name="PathSoFar">Full path to the registry key</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ static HRESULT RenderRegistryKeyToPolFormat
(
    _In_ HANDLE               OutFileHandle,
    _In_ RegistryKey  const & RegKey,
    _In_ std::wstring const & PathSoFar
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

    if (RegKey.Values.empty())
    {
        RegistryValue EmptyValue;
        EmptyValue.Type = 0;
        EmptyValue.Name.clear();
        EmptyValue.BinaryValue.clear();
        Result = RenderRegistryValueToPolFormat(OutFileHandle, NewPath, EmptyValue);
        if (FAILED(Result))
        {
            ReportError(E_HANDLE, L"Could not render empty registry value");
            return Result;
        }
    }

    for (const RegistryValue& Value : RegKey.Values)
    {
        Result = RenderRegistryValueToPolFormat(OutFileHandle, NewPath, Value);
        if (FAILED(Result))
        {
            ReportError(E_HANDLE, L"Could not render registry value" + Value.Name);
            return Result;
        }
    }

    for (const RegistryKey& Key : RegKey.Subkeys)
    {
        Result = RenderRegistryKeyToPolFormat(OutFileHandle, Key, NewPath);
        if (FAILED(Result))
        {
            ReportError(E_HANDLE, L"Could not render registry key" + Key.Name);
            return Result;
        }
    }

    return S_OK;
}

_Must_inspect_result_ HRESULT InternalToPolfile
(
    _In_ RegistryKey  const & RegKey,
    _In_ std::wstring const & OutputFilePath
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
        Result = WriteStringBufferToFile(OutFileHandle, Constants::PolFiles::Preamble);
        if (FAILED(Result))
        {
            ReportError(Result, L"Could not write preamble to PReg file");
            goto Cleanup;
        }
        Result = WritePODToFile(OutFileHandle, Constants::PolFiles::ExpectedVersion);
        if (FAILED(Result))
        {
            ReportError(Result, L"Could not write version to PReg file");
            goto Cleanup;
        }
    }

    for (const RegistryKey& k : RegKey.Subkeys)
    {
        Result = RenderRegistryKeyToPolFormat(OutFileHandle, k, std::wstring{});
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