// (C) Stormshield 2026
// Licensed under the Apache license, version 2.0
// See LICENSE.txt for details

#pragma once
#include <Windows.h>
#include <string>
#include <string_view>

/// <summary>
/// Display an HRESULT error on stderr
/// </summary>
/// <param name="ErrorCode">Error code</param>
/// <param name="Context">Some optional description of the context in which the error was encountered</param>
void ReportError
(
    _In_ HRESULT                   ErrorCode,
    _In_ std::wstring_view const & Context = std::wstring_view{L""}
);

#pragma region File ⟷ in-memory objects conversions

/// <summary>
/// Read a file as a binary stream and store its contents in a std::string object (see it as a byte sequence and not a stream).
/// </summary>
/// <param name="FilePath">Path of the file to read</param>
/// <param name="FileContents">Byte string representing the contents of the file</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT ReadFileToString
(
    _In_  std::wstring const & FilePath,
    _Out_ std::string        & FileContents
);

/// <summary>
/// Read a file as a binary stream and store its contents in a std::wstring object. The file should have a size multiple of sizeof(wchar_t).
/// </summary>
/// <remarks>The byte sequence is preserved, nothing is done to support any specific endianness. The file and wchar_t should preferably use the same endianness settings.</remarks>
/// <param name="FilePath">Path of the file to read</param>
/// <param name="FileContents">Byte string representing the contents of the file</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success</returns>
_Must_inspect_result_ HRESULT ReadFileToWString
(
    _In_  std::wstring const & FilePath,
    _Out_ std::wstring       & FileContents
);

/// <summary>
/// Append binary contents of a std::string to an opened file
/// </summary>
/// <param name="OutFileHandle">Handle to the file</param>
/// <param name="String">Input string</param>
/// <remarks> The string buffer is dumped to the file “as is”, one byte per character.
///       Null characters may be appended to the file if the string buffer includes some of them.</remarks>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT WriteStringBufferToFile
(
    _In_ HANDLE                   OutFileHandle,
    _In_ std::string_view const & String
);

/// <summary>
/// Append binary contents of a std::wstring to an opened file
/// </summary>
/// <param name="OutFileHandle">Handle to the file</param>
/// <param name="String">Input string</param>
/// <remarks>The string buffer is dumped to the file “as is”, using internal wchar_t representation as it lies in memory.
/// Null characters may be appended to the file if the string buffer includes some of them.
/// </remarks>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT WriteStringBufferToFile
(
    _In_ HANDLE                    OutFileHandle,
    _In_ std::wstring_view const & String
);

/// <summary>
/// Append a value to a file in binary representation, keeping memory layout
/// </summary>
/// <param name="OutFileHandle">Handle to the output file</param>
/// <param name="Value">Value to be appended</param>
/// <returns>Return value follows HRESULT semantics. Use the SUCCEEDED() or FAILED() macros to test success.</returns>
_Must_inspect_result_ HRESULT WritePODToFile
(
    _In_ HANDLE                    OutFileHandle,
    _In_ std::regular auto const & Value
)
{
    using ValueType = std::remove_reference_t<decltype(Value)>;
    DWORD BytesWritten = 0;
    if (!WriteFile(OutFileHandle, (LPCVOID)&Value, sizeof(ValueType), &BytesWritten, NULL))
    {
        HRESULT Result = HRESULT_FROM_WIN32(GetLastError());
        ReportError(Result, L"Could not write to output file");
        return Result;
    }
    if (BytesWritten != sizeof(ValueType))
    {
        HRESULT Result = E_UNEXPECTED;
        ReportError(Result, L"Bytes not fully written to file");
        return Result;
    }
    return S_OK;
}

#pragma endregion

#pragma region In-memory sequences helpers

/// <summary>
/// Given a view of in-memory elements, consume the first elements
/// </summary>
/// <typeparam name="T">Type of elements</typeparam>
/// <param name="v">The view</param>
/// <param name="CharCount">How many chars are to be consumed</param>
template<typename T> void AdvanceReadHead
(
    _Inout_ std::basic_string_view<T> & v,
    _In_    size_t                      CharCount
)
{
    v.remove_prefix(CharCount);
}

/// <summary>
/// Given a view of in-memory elements, check that it starts with a given element, and, if so, consume it
/// </summary>
/// <typeparam name="T">Type of elements</typeparam>
/// <param name="v">The view</param>
/// <param name="ExpectedChar">The element expected at view beginning</param>
/// <returns>true if the element was found and the view updated, false otherwise.</returns>
template<std::integral T> _Check_return_ bool ExpectAndConsumeAtReadHead
(
    _Inout_ std::basic_string_view<T> & v,
    _In_    T                           ExpectedChar
)
{
    bool ReturnValue = !v.empty() && v[0] == ExpectedChar;
    if (ReturnValue)
    {
        v.remove_prefix(1);
    }
    return ReturnValue;
}

/// <summary>
/// Given a view of in-memory elements, check that it starts with a given sequence, and, if so, consume it
/// </summary>
/// <typeparam name="T">Type of elements</typeparam>
/// <param name="v">The view</param>
/// <param name="ExpectedChar">The element sequence expected at view beginning</param>
/// <returns>true if the element sequence was found and the view updated, false otherwise.</returns>
template<std::integral T> _Check_return_ bool ExpectAndConsumeAtReadHead
(
    _Inout_ std::basic_string_view<T>       & v,
    _In_    std::basic_string_view<T> const & ExpectedString
)
{
    bool ReturnValue = v.length() >= ExpectedString.length() && std::equal(ExpectedString.cbegin(), ExpectedString.cend(), v.cbegin());
    if (ReturnValue)
    {
        v.remove_prefix(ExpectedString.length());
    }
    return ReturnValue;
}

/// <summary>
/// Given a view of in-memory bytes, check that it contains at start the in-memory
/// representation of a given integral value, and, if so, consume it
/// </summary>
/// <param name="v">The view</param>
/// <param name="ExpectedValue">The value that is checked for presence at view start</param>
/// <returns>true if the value was found and the view was updated, false otherwise</returns>
bool ExpectAndConsumePODAtReadHead
(
    _Inout_ std::string_view  & v,
    _In_    std::regular auto   ExpectedValue
) noexcept
{
    using ValueType = decltype(ExpectedValue);

    if (v.size() >= sizeof(ValueType)) {
        auto RealValue = ValueType{};
        memcpy(&RealValue, &v[0], sizeof(ValueType));
        if (ExpectedValue == RealValue)
        {
            v.remove_prefix(sizeof(ValueType));
            return true;
        }
    }
    return false;
}

/// <summary>
/// Given a view of in-memory bytes, try to retrieve a plain old data object at the beginning of the view, and advance the view
/// </summary>
/// <param name="v">The view</param>
/// <param name="Value">The value to retrieve</param>
/// <returns>true if the view had a greater size than the object to retrieve, and was updated; false otherwise.</returns>
_Check_return_ bool RetrieveAndConsumePODAtReadHead
(
    _Inout_ std::string_view  & v,
    _Out_   std::regular auto & Value
) noexcept
{
    using ValueType = std::remove_reference_t<decltype(Value)>;
    if (v.size() >= sizeof(ValueType))
    {
        CopyMemory(&Value, v.data(), sizeof(ValueType));
        v.remove_prefix(sizeof(ValueType));
        return true;
    }
    return false;
}

/// <summary>
/// Given a sequence of in-memory bytes, reinterpret it as a sequence of wchar_t elements.
/// </summary>
/// <remarks>Depending on the initial size, some trailing elements could be truncated.</remarks>
/// <param name="v">The byte sequence view</param>
/// <returns>A view of a wchar_t sequence holding the same elements.</returns>
std::wstring_view GetWstringViewFromStringView
(
    _In_ std::string_view const & v
) noexcept;

#pragma endregion

/// <summary>
/// Delete .LOG1 and .LOG2 system files that were created when loading an application hive
/// </summary>
/// <param name="HiveFilePath">Path to the hive file</param>
void DeleteHiveLogFiles
(
    _In_ std::wstring_view const & HiveFilePath
);

/// <summary>
/// Replace globally a substring with a replacement in a string
/// </summary>
/// <param name="String">String in which to perform replacements</param>
/// <param name="Pattern">Pattern to search in the input string. Pattern should not be empty, otherwise the function does nothing.</param>
/// <param name="Replacement">Replacement to put in place of the pattern</param>
void GlobalStringSubstitute
(
    _Inout_ std::wstring       & String,
    _In_    std::wstring const & Pattern,
    _In_    std::wstring const & Replacement
);
