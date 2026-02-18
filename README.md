# HiveSwarming

Conversions between registry hive and registry export formats without a need
for special privileges.

[Changelog](https://github.com/stormshield/HiveSwarming/blob/master/CHANGELOG.md) - [Latest version](https://github.com/stormshield/HiveSwarming/releases/download/v1.6/HiveSwarming.exe)

© 2026 Stormshield

## Usage

```
HiveSwarming.exe --from <format> --to <format> <input_file> <output_file>
```

Supported values for `<format>`:
* *`reg`*  : *.reg* format as produced by the Registry Editor
* *`hive`* : binary format used by MS Windows
* *`pol`*  : *.pol* format used in Group Policy DataStores
* *`reg+`* : *.reg* format with extensions for readability of `REG_MULTI_SZ`,
  `REG_EXPAND_SZ` and `REG_QWORD` values.  This format is NOT recognized by the
  standard tools!

Exit code 0 means success, other values mean failure.

Failure will be accompanied by some output on standard error.

## FAQ

**Why this name?**

Registry vocabulary tends to follow beekeeping terms. Swarming is a term
that perfectly applies to a migration of your data.

See also [Raymond Chen's explanation](https://devblogs.microsoft.com/oldnewthing/20030808-00/?p=42943).

**Any limitations?**

Yes. First, when you load a hive file using the API `RegLoadAppKeyW`, all
security descriptors for all keys inside the hive should be identical.
Otherwise it will fail.

Second, if a key name contains a closing bracket followed by a newline
character, your *.reg* file is not parseable. This limitation is also valid for
standard *.reg* files

Third, when converting from *.reg* file to a hive, any key containing a single
value named `SymbolicLinkValue` and of type `REG_LINK` will be recreated as a
symbolic link. This should be what is expected most of the time.

**Is the *.reg* file compatible with `reg.exe import`?**

Mostly. The generated *.reg* file has `[(HiveRoot)]` as root key. You will have
to substitute it globally to make it importable at any desired location.  When
converting back, it is not necessary to keep this name for the root key, but a
requirement is that all keys descend of the first one.

**What are requirements for *.reg* files?**

*.reg* files must:
* be encoded as UTF-16, Little-Endian, with a Byte Order Mark
* Use `\r\n` for line endings
* Start with `Windows Registry Editor Version 5.00` and at least one blank line
* Have non-empty root key (first key) name
* Have all keys be descendants of the root key
* Have no trailing or leading spaces on lines
* Have no blank lines between a key and its last value (except inside string
  values when the string themselves contain blank lines)
* Have a blank line after the last value of a key (including last key)
* Be importable to the registry

Some third party software will generate invalid files. For example double
quotes inside strings will be left unescaped; export file encoding will use a
multi-byte character set, and Unicode data will be lost. Those files are not
supported.

**Some random system, hidden files appeared in the process.**

This is a consequence of the internals of registry hives. We try to delete
these files once the conversions are done, but it is possible that something
failed (handle still opened for example). You may probably delete these files
themselves if your hive is not mounted anywhere and not in use by any process.

## Contribute

Your pull requests are welcome and will be reviewed.

## Privacy pledge

This product works offline. It will not use the network other than for
accessing the files you specified on the command line.

## AI policy

This product was written, reviewed and tested by humans.

It will never interact with LLMs at runtime.

You may use AI agents to find problems and get code change suggestions, but
please have a human involved in submitting issues or merge requests.

## Get it

Hiveswarming is a standalone executable. You can just download any version from
the Github release pages. The executable should be digitally signed.

As of August 2025, Hiveswarming v1.5 has been made available on WinGet by a
[third-party user contribution](https://github.com/microsoft/winget-pkgs/pull/280221).

You can install this specific version with:

```
winget install Hiveswarming --exact --id HiveSwarming --version 1.5.0.0
```

