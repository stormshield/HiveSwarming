(C) 2023 Stormshield

HiveSwarming - Conversions between registry hive and registry export formats
  without a need for special privileges.

USAGE
-----

HiveSwarming.exe --reg-file-to-hive <export.reg> <hive_file>
HiveSwarming.exe --hive-to-reg-file <hive_file> <export.reg>

EXIT CODE
---------
0 means success, other values mean failure.
Failure will be accompanied by some output on standard error.

FAQ
---

Q. Why this name?
A. Registry vocabulary tends to follow beekeeping terms. Swarming is a term
   that perfectly applies to a migration of your data.

   See also: https://devblogs.microsoft.com/oldnewthing/20030808-00/?p=42943

Q. Any limitations?
A. Yes. First, when you load a hive file using the API RegLoadAppKeyW, all
   security descriptors for all keys inside the hive should be identical.
   Otherwise it will fail.
   Second, if a key name contains a closing bracket followed by a newline
   character, your .reg file is not parseable. This limitation is also valid
   for standard .reg files
   Third, when converting from .reg file to a hive, any key containing a single
   value named "SymbolicLinkValue" and of type REG_LINK will be recreated as a
   symbolic link. This should be what is expected most of the time.

Q. Is the .reg file compatible with reg.exe import?
A. Mostly. The generated .reg file has [(HiveRoot)] as root key. You will
   have to substitute it globally to make it importable at any desired location.
   When converting back, it is not necessary to keep this name for the root key,
   but a requirement is that all keys descend of the first one.

Q. What are requirements for .reg files?
A. .reg files must:
    - be encoded as UTF-16, Little-Endian, with a Byte Order Mark
    - Use \r\n for line endings
    - Start with "Windows Registry Editor Version 5.00" and at least one blank
      line
    - Have non-empty root key (first key) name
    - Have all keys be descendants of the root key
    - Have no trailing or leading spaces on lines
    - Have no blank lines between a key and its last value (except inside
      string values when the string themselves contain blank lines)
    - Have a blank line after the last value of a key (including last key)
    - Be importable to the registry
   Some third party software, like RegView, will generate invalid files. For
   example double quotes inside strings will be left unescaped; export file
   encoding will use a multi-byte character set, and Unicode data will be lost.
   Those files are not supported.

Q. Some random system, hidden files appeared in the process.
A. This is a consequence of the internals of registry hives. We try to delete
   these files once the conversions are done, but it is possible that
   something failed (HANDLE still opened for example). You may probably delete
   these files themselves if your hive is not mounted anywhere and not in use
   by any process.

Q. Do you accept pull requests?
A. They are welcome and will be reviewed.
