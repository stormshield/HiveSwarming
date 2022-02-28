// (C) Stormshield 2022
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include <windows.h>
#include <iostream>
#include "Conversions.h"
#include "CommonFunctions.h"
#include "Constants.h"

/// @brief Program entry point
/// @param[in] Argc Command line token count, including program name
/// @param[in] Argv Tokens of the command line (#Argc valid entries)
/// @retval EXIT_SUCCESS Program execution successful
/// @retval EXIT_FAILURE Program execution failed
int wmain(
    INT CONST Argc,
    LPCWSTR CONST* CONST Argv
)
{
    HRESULT Result = E_FAIL;

    RegistryKey InternalStruct;

    auto Usage = [&]()
    {
        std::wcerr << L"Usage: " << std::endl <<
            L"\t" << Argv[0] << L" " << Constants::Program::HiveToRegFileSwitch << L" <HiveFile> <RegFile>" << std::endl <<
            L"\t" << Argv[0] << L" " << Constants::Program::RegFileToHiveSwitch << L" <RegFile> <HiveFile>" << std::endl <<
            std::endl;
    };

    if (Argc <= 1)
    {
        Usage();
        Result = S_OK;
        goto Cleanup;
    }

    if (Constants::Program::HiveToRegFileSwitch == Argv[1])
    {
        if (Argc != 4)
        {
            Usage();
            Result = E_INVALIDARG;
            goto Cleanup;
        }

        const std::wstring HivePath { Argv[2] };
        const std::wstring RegPath { Argv[3] };

        Result = HiveToInternal(HivePath, Constants::Defaults::ExportKeyPath, InternalStruct);
        if (FAILED(Result))
        {
            goto Cleanup;
        }

        Result = InternalToRegfile(InternalStruct, RegPath);
        if (FAILED(Result))
        {
            goto Cleanup;
        }
    }
    else if (Constants::Program::RegFileToHiveSwitch == Argv[1])
    {
        if (Argc != 4)
        {
            Usage();
            Result = E_INVALIDARG;
            goto Cleanup;
        }

        const std::wstring RegPath { Argv[2] };
        const std::wstring HivePath { Argv[3] };

        Result = RegfileToInternal(RegPath, InternalStruct);
        if (FAILED(Result))
        {
            ReportError(Result, L"Serializing registry file" + RegPath);
            goto Cleanup;
        }

        Result = InternalToHive(InternalStruct, HivePath);
        if (FAILED(Result))
        {
            ReportError(Result, L"Writing hive file " + HivePath);
            goto Cleanup;
        }
    }
    else
    {
        Usage();
        Result = E_INVALIDARG;
        goto Cleanup;
    }

    Result = S_OK;

Cleanup:
    return FAILED(Result) ? EXIT_FAILURE : EXIT_SUCCESS;
}