// (C) Stormshield 2025
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#include <windows.h>
#include <iostream>
#include "Conversions.h"
#include "CommonFunctions.h"
#include "Constants.h"
#include <span>

/// <summary>
/// Formats that Hiveswarming supports
/// </summary>
enum class SupportedFormat {
    /// <summary>
    /// Unspecified format
    /// </summary>
    Unknown,

    /// <summary>
    /// Registry hive
    /// </summary>
    Hive,

    /// <summary>
    /// Registry text file
    /// </summary>
    Reg,

    /// <summary>
    /// Registry text file, with Hiveswarming extensions
    /// </summary>
    RegWithHiveswarmingExtensions,

    /// <summary>
    /// Security policy file
    /// </summary>
    Pol
};

/// <summary>
/// Convert a command-line argument to a supported registry format
/// </summary>
/// <param name="Format">Command line argument</param>
/// <returns>Enum value for the format</returns>
static SupportedFormat ArgToFormat
(
    std::wstring const & Format
)
{
    if (Format == Constants::Program::FormatHive)
        return SupportedFormat::Hive;
    else if (Format == Constants::Program::FormatReg)
        return SupportedFormat::Reg;
    else if (Format == Constants::Program::FormatRegWithHiveswarmingExtensions)
        return SupportedFormat::RegWithHiveswarmingExtensions;
    else if (Format == Constants::Program::FormatPol)
        return SupportedFormat::Pol;
    else
        return SupportedFormat::Unknown;
}

/// <summary>
/// Entry point for the program
/// </summary>
/// <param name="Argc">Number of parameters in command line, including program invocation</param>
/// <param name="Argv">Command-line arguments</param>
/// <returns>EXIT_SUCCESS or EXIT_FAILURE</returns>
int wmain(
    int             Argc,
    LPCWSTR const * Argv
)
{
    HRESULT Result = E_FAIL;
    SupportedFormat InputFormat = SupportedFormat::Unknown;
    SupportedFormat OutputFormat = SupportedFormat::Unknown;
    std::wstring InputFile{};
    std::wstring OutputFile{};
    RegistryKey InternalStruct;

    auto Usage = [&]()
    {
        std::wcerr << L"Usage: " << std::endl
            << L"\t" << Argv[0] << L" " << Constants::Program::FromSwitch << " <input format> "
                                        << Constants::Program::ToSwitch << " <output format> "
                                        << "<InputFile> <OutputFile>" << std::endl
            << std::endl
            << L"\t\tSupported formats:" << std::endl
            << L"\t\t\t* " << Constants::Program::FormatHive << " (Registry hive format)" << std::endl
            << L"\t\t\t* " << Constants::Program::FormatReg << " (Microsoft registry format)" << std::endl
            << L"\t\t\t* " << Constants::Program::FormatRegWithHiveswarmingExtensions << " (Registry format with Hiweswarming extensions for readability)" << std::endl
            << L"\t\t\t* " << Constants::Program::FormatPol << " (Registry Policy Message Syntax)" << std::endl;
    };

    if (Argc <= 1)
    {
        Usage();
        Result = S_OK;
        goto Cleanup;
    }

    for (size_t ArgIndex = 1; ArgIndex < Argc;)
    {
        if (Argv[ArgIndex] == Constants::Program::FromSwitch)
        {
            ArgIndex++;
            if (ArgIndex >= Argc) {
                Usage();
                Result = E_INVALIDARG;
                goto Cleanup;
            }
            InputFormat = ArgToFormat(Argv[ArgIndex]);
            ArgIndex++;
            continue;
        }
        else if (Argv[ArgIndex] == Constants::Program::ToSwitch)
        {
            ArgIndex++;
            if (ArgIndex >= Argc) {
                Usage();
                Result = E_INVALIDARG;
                goto Cleanup;
            }
            OutputFormat = ArgToFormat(Argv[ArgIndex]);
            ArgIndex++;
            continue;
        }
        else if (InputFile.empty())
        {
            InputFile = Argv[ArgIndex];
            ArgIndex++;
            continue;
        }
        else if (OutputFile.empty())
        {
            OutputFile = Argv[ArgIndex];
            ArgIndex++;
            continue;
        }
        else
        {
            Usage();
            Result = E_INVALIDARG;
            goto Cleanup;
        }
    }

    switch (InputFormat)
    {
    case SupportedFormat::Hive:
        Result = HiveToInternal(InputFile, Constants::Defaults::ExportKeyPath, InternalStruct);
        break;
    case SupportedFormat::Reg:
    case SupportedFormat::RegWithHiveswarmingExtensions:
        Result = RegfileToInternal(InputFile, InternalStruct);
        break;
    case SupportedFormat::Pol:
        Result = PolfileToInternal(InputFile, Constants::Defaults::ExportKeyPath, InternalStruct);
        break;
    default:
        Result = E_UNEXPECTED;
        break;
    }
    if (FAILED(Result))
    {
        ReportError(Result, L"Reading input file " + InputFile);
        goto Cleanup;
    }

    switch (OutputFormat)
    {
    case SupportedFormat::Hive:
        Result = InternalToHive(InternalStruct, OutputFile);
        break;
    case SupportedFormat::Reg:
        Result = InternalToRegfile(InternalStruct, OutputFile, false);
        break;
    case SupportedFormat::RegWithHiveswarmingExtensions:
        Result = InternalToRegfile(InternalStruct, OutputFile, true);
        break;
    case SupportedFormat::Pol:
        Result = InternalToPolfile(InternalStruct, OutputFile);
        break;
    default:
        Result = E_UNEXPECTED;
        break;
    }
    if (FAILED(Result))
    {
        ReportError(Result, L"Writing output file " + OutputFile);
        goto Cleanup;
    }

    Result = S_OK;

Cleanup:
    return FAILED(Result) ? EXIT_FAILURE : EXIT_SUCCESS;
}