# scanr

/**
 * A basic grep-like command-line tool for Windows.
 * Searches for a pattern in specified files or recursively in directories.
 * Supports an option for case-insensitive search.
 *
 * Usage:
 * scanr [-i] <pattern> <path>
 *
 * Arguments:
 * -i        : (Optional) Perform case-insensitive matching.
 * <pattern> : The text pattern to search for.
 * <path>    : The file or directory to search within. If it's a directory,
 * the search will be recursive.
 *
 * Example (Case-Sensitive):
 * scanr "hello world" C:\Users\Public\Documents
 * scanr "important_function" my_code.cpp
 *
 * Example (Case-Insensitive):
 * scanr -i "error" C:\Logs
 *
 * Compilation (using g++ with C++17 support):
 * g++ scanr.cpp -o scanr.exe -std=c++17 -static-libgcc -static-libstdc++
 *
 * Compilation (using Visual Studio Developer Command Prompt):
 * cl /std:c++17 /EHsc scanr.cpp /Fe:scanr.exe
 *
 * Note: Requires a C++17 compliant compiler for <filesystem>.
 */