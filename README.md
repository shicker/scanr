# Scanr - A Grep Clone for Windows

Scanr is a powerful and flexible command-line tool for searching text patterns in files and directories. Inspired by the Unix `grep` utility, Scanr is designed to work seamlessly on Windows systems, offering advanced search capabilities with support for regular expressions, case-insensitive matching, context lines, and more.

---

## Features

- **Pattern Matching**: Search for text patterns using simple strings or extended regular expressions (ERE).
- **Case Sensitivity**: Perform case-sensitive or case-insensitive searches.
- **Context Lines**: Display lines before and after matches for better context (`-A`, `-B`, `-C`).
- **File Filtering**: Search specific file types using extensions (`-e`).
- **Line Numbers**: Display line numbers for matches (`-n`).
- **Invert Match**: Select lines that do not match the pattern (`-v`).
- **Count Matches**: Count the number of matching lines (`-c`).
- **List Filenames**: Display only the names of files containing matches (`-l`).
- **Standard Input Support**: Process input from standard input (stdin).
- **Multiple Patterns**: Search for multiple patterns using `-e` or pattern files (`-f`).
- **Windows Compatibility**: Fully compatible with Windows file systems and paths.

---

## Installation

### Prerequisites
- A C++17-compatible compiler (e.g., GCC, Clang, or MSVC).
- Windows operating system.

### Build Instructions
1. Clone the repository:
   ```bash
   git clone https://github.com/shicker/scanr.git
   cd scanr
   ```
2. Compile the source code:
   ```bash
   g++ -std=c++17 -o scanr scanr.cpp
   ```
3. Run the executable:
   ```bash
   ./scanr
   ```

---

### Make Scanr Available System-wide (Like `grep` on Linux)

To use `scanr` from any command prompt window, add its folder to your system `PATH`:

1. **Build `scanr.exe`** as shown above.
2. **Copy `scanr.exe`** to a folder of your choice (e.g., `C:\Tools\scanr`).
3. **Add the folder to your Windows `PATH`:**
   - Press <kbd>Win</kbd> + <kbd>R</kbd>, type `sysdm.cpl`, and press <kbd>Enter</kbd>.
   - Go to the **Advanced** tab and click **Environment Variables**.
   - Under **System variables**, select `Path` and click **Edit**.
   - Click **New** and enter the path to the folder containing `scanr.exe` (e.g., `C:\Tools\scanr`).
   - Click **OK** to close all dialogs.
4. **Open a new Command Prompt** and type:
   ```
   scanr --help
   ```
   If you see the usage message, `scanr` is now available system-wide.

---

## Usage

### Basic Syntax
```bash
scanr [OPTIONS]... PATTERN [FILE]...
```

### Examples
1. **Search for a pattern in a file**:
   ```bash
   scanr "error" log.txt
   ```
2. **Case-insensitive search**:
   ```bash
   scanr -i "warning" log.txt
   ```
3. **Search recursively in a directory**:
   ```bash
   scanr "TODO" C:\Projects
   ```
4. **Display line numbers**:
   ```bash
   scanr -n "main" source.cpp
   ```
5. **Show context lines**:
   ```bash
   scanr -C 2 "function" source.cpp
   ```
6. **List filenames with matches**:
   ```bash
   scanr -l "pattern" *.txt
   ```
7. **Use regular expressions**:
   ```bash
   scanr -E "error|warning" log.txt
   ```
8. **Search multiple patterns**:
   ```bash
   scanr -e "error" -e "warning" log.txt
   ```
9. **Read patterns from a file**:
   ```bash
   scanr -f patterns.txt log.txt
   ```

---

## Options

| Option                  | Description                                                                 |
|-------------------------|-----------------------------------------------------------------------------|
| `-c, --count`           | Print only the count of matching lines per file.                           |
| `-h`                    | Suppress the prefixing of filenames on output.                             |
| `-i`                    | Ignore case distinctions in patterns and input.                            |
| `-l`                    | Print only the names of files containing matches.                          |
| `-n`                    | Prefix each line of output with its line number.                           |
| `-v`                    | Invert the match, selecting non-matching lines.                            |
| `-e PATTERN`            | Use PATTERN for matching (can be used multiple times).                     |
| `-f FILE`               | Read patterns from FILE, one per line.                                     |
| `-E`                    | Interpret PATTERN as an extended regular expression (ERE).                |
| `-w`                    | Match only whole words (forces `-E`).                                      |
| `-o`                    | Print only the matched parts of lines (forces `-E`).                      |
| `-A NUM`                | Print NUM lines of trailing context after each match.                      |
| `-B NUM`                | Print NUM lines of leading context before each match.                      |
| `-C NUM`                | Print NUM lines of output context (equivalent to `-A NUM -B NUM`).         |

---

## Error Handling

- If a file cannot be opened, Scanr will display an error message and continue processing other files.
- Invalid regular expressions or patterns will result in an error message, and the program will terminate.

---

## Contributing

Contributions are welcome! If you encounter a bug, have a feature request, or want to contribute code, please open an issue or submit a pull request on GitHub.

---

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

---

## Acknowledgments

Scanr is inspired by the Unix `grep` utility and aims to bring similar functionality to Windows users with additional features and modern C++ practices.