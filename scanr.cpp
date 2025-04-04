#include <iostream>
#include <fstream>
#include <regex>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstring>
#include <getopt.h>

namespace fs = std::filesystem;

class ScanrOptions {
public:
    bool ignoreCase = false;
    bool lineNumbers = false;
    bool recursiveSearch = false;
    bool invertMatch = false;
    bool wholeWord = false;
    bool countOnly = false;
    bool useRegex = false;
    bool colorOutput = true;
    int contextLines = 0;
    std::string pattern;
    std::vector<std::string> filePaths;
    int threadCount = std::thread::hardware_concurrency();
};

class ConsoleColors {
public:
    static const std::string reset;
    static const std::string red;
    static const std::string green;
    static const std::string yellow;
    static const std::string blue;
};

const std::string ConsoleColors::reset = "\033[0m";
const std::string ConsoleColors::red = "\033[31m";
const std::string ConsoleColors::green = "\033[32m";
const std::string ConsoleColors::yellow = "\033[33m";
const std::string ConsoleColors::blue = "\033[34m";

class MatchResult {
public:
    std::string filename;
    int lineNumber;
    std::string line;
    std::vector<std::pair<int, int>> matchPositions; // start, length

    MatchResult(const std::string& filename, int lineNumber, const std::string& line)
        : filename(filename), lineNumber(lineNumber), line(line) {}
};

std::mutex outputMutex;
std::atomic<int> matchCount(0);

void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " [OPTIONS] PATTERN [FILE...]\n"
              << "Search for PATTERN in each FILE or standard input.\n"
              << "Example: " << programName << " -i 'hello world' file.txt\n\n"
              << "Options:\n"
              << "  -i, --ignore-case     Ignore case distinctions in PATTERN\n"
              << "  -n, --line-number     Print line number with output lines\n"
              << "  -r, --recursive       Read all files under each directory, recursively\n"
              << "  -v, --invert-match    Select non-matching lines\n"
              << "  -w, --word-regexp     Match whole words only\n"
              << "  -c, --count           Print only a count of matching lines per FILE\n"
              << "  -e, --regexp          PATTERN is a regular expression\n"
              << "  -A, --after-context=NUM   Print NUM lines of trailing context after matching lines\n"
              << "  -B, --before-context=NUM  Print NUM lines of leading context before matching lines\n"
              << "  -C, --context=NUM     Print NUM lines of output context\n"
              << "  --color[=WHEN]        Colorize the output; WHEN can be 'always', 'never', or 'auto'\n"
              << "  -j, --jobs=NUM        Use NUM threads for searching\n"
              << "  -h, --help            Display this help and exit\n"
              << std::endl;
}

ScanrOptions parseOptions(int argc, char* argv[]) {
    ScanrOptions options;
    
    static struct option long_options[] = {
        {"ignore-case", no_argument, 0, 'i'},
        {"line-number", no_argument, 0, 'n'},
        {"recursive", no_argument, 0, 'r'},
        {"invert-match", no_argument, 0, 'v'},
        {"word-regexp", no_argument, 0, 'w'},
        {"count", no_argument, 0, 'c'},
        {"regexp", no_argument, 0, 'e'},
        {"after-context", required_argument, 0, 'A'},
        {"before-context", required_argument, 0, 'B'},
        {"context", required_argument, 0, 'C'},
        {"color", optional_argument, 0, 0},
        {"jobs", required_argument, 0, 'j'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "invwcerA:B:C:j:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 0:
                if (strcmp(long_options[option_index].name, "color") == 0) {
                    if (optarg) {
                        if (strcmp(optarg, "never") == 0) {
                            options.colorOutput = false;
                        } else if (strcmp(optarg, "always") == 0) {
                            options.colorOutput = true;
                        }
                    }
                }
                break;
            case 'i':
                options.ignoreCase = true;
                break;
            case 'n':
                options.lineNumbers = true;
                break;
            case 'r':
                options.recursiveSearch = true;
                break;
            case 'v':
                options.invertMatch = true;
                break;
            case 'w':
                options.wholeWord = true;
                break;
            case 'c':
                options.countOnly = true;
                break;
            case 'e':
                options.useRegex = true;
                break;
            case 'A':
            case 'B':
            case 'C':
                options.contextLines = std::stoi(optarg);
                break;
            case 'j':
                options.threadCount = std::stoi(optarg);
                break;
            case 'h':
                printUsage(argv[0]);
                exit(0);
            case '?':
                printUsage(argv[0]);
                exit(1);
            default:
                abort();
        }
    }
    
    if (optind < argc) {
        options.pattern = argv[optind++];
        
        while (optind < argc) {
            options.filePaths.push_back(argv[optind++]);
        }
    } else {
        std::cerr << "Missing required pattern argument." << std::endl;
        printUsage(argv[0]);
        exit(1);
    }
    
    // If no files specified, use stdin
    if (options.filePaths.empty() && !options.recursiveSearch) {
        options.filePaths.push_back("-");  // Convention for stdin
    }
    
    return options;
}

std::regex buildRegex(const ScanrOptions& options) {
    std::string pattern = options.pattern;
    
    if (!options.useRegex) {
        // Escape regex special characters if not using regex mode
        static const std::string specialChars = "\\^$.|?*+()[{";
        for (size_t i = 0; i < pattern.length(); ++i) {
            if (specialChars.find(pattern[i]) != std::string::npos) {
                pattern.insert(i, "\\");
                ++i;
            }
        }
    }
    
    if (options.wholeWord) {
        pattern = "\\b" + pattern + "\\b";
    }
    
    std::regex::flag_type flags = std::regex::ECMAScript;
    if (options.ignoreCase) {
        flags |= std::regex::icase;
    }
    
    return std::regex(pattern, flags);
}

std::vector<std::pair<int, int>> findMatchPositions(const std::string& line, const std::regex& regex) {
    std::vector<std::pair<int, int>> positions;
    auto words_begin = std::sregex_iterator(line.begin(), line.end(), regex);
    auto words_end = std::sregex_iterator();
    
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        positions.push_back(std::make_pair(match.position(), match.length()));
    }
    
    return positions;
}

std::string highlightMatches(const std::string& line, const std::vector<std::pair<int, int>>& positions, bool colorOutput) {
    if (!colorOutput || positions.empty()) {
        return line;
    }
    
    std::string result;
    size_t lastPos = 0;
    
    for (const auto& pos : positions) {
        result += line.substr(lastPos, pos.first - lastPos);
        result += ConsoleColors::red + line.substr(pos.first, pos.second) + ConsoleColors::reset;
        lastPos = pos.first + pos.second;
    }
    
    result += line.substr(lastPos);
    return result;
}

void printMatch(const MatchResult& result, const ScanrOptions& options, 
                const std::vector<std::string>& contextBefore,
                const std::vector<std::string>& contextAfter) {
    std::lock_guard<std::mutex> lock(outputMutex);
    
    // Print filename if more than one file
    if (options.filePaths.size() > 1 || options.recursiveSearch) {
        std::cout << ConsoleColors::green << result.filename << ConsoleColors::reset << ":";
    }
    
    // Print line number if requested
    if (options.lineNumbers) {
        std::cout << ConsoleColors::blue << result.lineNumber << ConsoleColors::reset << ":";
    }
    
    // Print context before if available
    for (const auto& ctxLine : contextBefore) {
        std::cout << ctxLine << std::endl;
    }
    
    // Print the matching line with highlighted matches
    std::cout << highlightMatches(result.line, result.matchPositions, options.colorOutput) << std::endl;
    
    // Print context after if available
    for (const auto& ctxLine : contextAfter) {
        std::cout << ctxLine << std::endl;
    }
    
    if (!contextAfter.empty()) {
        std::cout << "--" << std::endl;
    }
}

void searchFile(const std::string& filePath, const ScanrOptions& options, const std::regex& regex) {
    std::ifstream file;
    std::istream* input = &std::cin;
    bool isStdin = filePath == "-";
    
    if (!isStdin) {
        file.open(filePath);
        if (!file.is_open()) {
            std::cerr << "scanr: " << filePath << ": No such file or directory" << std::endl;
            return;
        }
        input = &file;
    }
    
    std::string line;
    int lineNumber = 0;
    int fileMatchCount = 0;
    
    // For context lines
    std::vector<std::string> contextLines;
    int contextCount = 0;
    
    // For context before and after (circular buffer)
    std::vector<std::string> beforeContext(options.contextLines);
    int beforeIndex = 0;
    
    while (std::getline(*input, line)) {
        ++lineNumber;
        
        // Find all matches in the line
        auto matchPositions = findMatchPositions(line, regex);
        bool hasMatch = !matchPositions.empty();
        
        // Apply invert match if needed
        if (options.invertMatch) {
            hasMatch = !hasMatch;
            matchPositions.clear(); // No highlighting for inverted matches
        }
        
        if (hasMatch) {
            ++fileMatchCount;
            ++matchCount;
            
            if (!options.countOnly) {
                // Prepare context lines before the match
                std::vector<std::string> before;
                if (options.contextLines > 0) {
                    for (int i = 0; i < options.contextLines; ++i) {
                        int idx = (beforeIndex + i) % options.contextLines;
                        if (!beforeContext[idx].empty()) {
                            if (options.lineNumbers) {
                                before.push_back(ConsoleColors::blue + 
                                    std::to_string(lineNumber - options.contextLines + i) + 
                                    ConsoleColors::reset + "-" + beforeContext[idx]);
                            } else {
                                before.push_back("-" + beforeContext[idx]);
                            }
                        }
                    }
                }
                
                // Prepare result
                MatchResult result(filePath, lineNumber, line);
                result.matchPositions = matchPositions;
                
                // Print match with context
                printMatch(result, options, before, {});
                
                // Reset context buffer
                contextCount = options.contextLines;
            }
        } else if (contextCount > 0 && !options.countOnly) {
            // This is a context line after a match
            std::lock_guard<std::mutex> lock(outputMutex);
            
            if (options.lineNumbers) {
                std::cout << ConsoleColors::blue << lineNumber << ConsoleColors::reset << "-";
            } else {
                std::cout << "-";
            }
            std::cout << line << std::endl;
            --contextCount;
            
            if (contextCount == 0) {
                std::cout << "--" << std::endl;
            }
        }
        
        // Store line for before-context
        if (options.contextLines > 0) {
            beforeContext[beforeIndex] = line;
            beforeIndex = (beforeIndex + 1) % options.contextLines;
        }
    }
    
    if (options.countOnly) {
        std::lock_guard<std::mutex> lock(outputMutex);
        if (options.filePaths.size() > 1 || options.recursiveSearch) {
            std::cout << filePath << ":";
        }
        std::cout << fileMatchCount << std::endl;
    }
    
    if (!isStdin) {
        file.close();
    }
}

void processFile(const std::string& path, const ScanrOptions& options, const std::regex& regex) {
    if (fs::is_directory(path)) {
        if (options.recursiveSearch) {
            try {
                for (const auto& entry : fs::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        searchFile(entry.path().string(), options, regex);
                    }
                }
            } catch (const fs::filesystem_error& ex) {
                std::cerr << "scanr: " << ex.what() << std::endl;
            }
        } else {
            std::cerr << "scanr: " << path << ": Is a directory" << std::endl;
        }
    } else {
        searchFile(path, options, regex);
    }
}

void searchWorker(std::vector<std::string> filePaths, const ScanrOptions& options, const std::regex& regex) {
    for (const auto& path : filePaths) {
        processFile(path, options, regex);
    }
}

int main(int argc, char* argv[]) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Parse command line options
    ScanrOptions options = parseOptions(argc, argv);
    
    // Build regex from pattern
    std::regex regex;
    try {
        regex = buildRegex(options);
    } catch (const std::regex_error& e) {
        std::cerr << "scanr: Invalid regular expression: " << e.what() << std::endl;
        return 1;
    }
    
    // Handle recursive directory search with no specific paths
    if (options.recursiveSearch && options.filePaths.empty()) {
        options.filePaths.push_back(".");
    }
    
    // Special case for stdin
    if (options.filePaths.size() == 1 && options.filePaths[0] == "-") {
        searchFile("-", options, regex);
        return 0;
    }
    
    // Multi-threading for file processing
    std::vector<std::thread> threads;
    size_t numThreads = std::min(options.threadCount, static_cast<int>(options.filePaths.size()));
    
    if (numThreads <= 1 || options.filePaths.empty()) {
        // Single thread operation
        for (const auto& path : options.filePaths) {
            processFile(path, options, regex);
        }
    } else {
        // Multi-thread operation - divide files among threads
        std::vector<std::vector<std::string>> threadFilePaths(numThreads);
        
        for (size_t i = 0; i < options.filePaths.size(); ++i) {
            threadFilePaths[i % numThreads].push_back(options.filePaths[i]);
        }
        
        for (size_t i = 0; i < numThreads; ++i) {
            threads.emplace_back(searchWorker, threadFilePaths[i], options, regex);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Print summary if more than one file
    if (!options.countOnly && (options.filePaths.size() > 1 || options.recursiveSearch)) {
        std::cout << "\nscanr: Found " << matchCount << " matches in " 
                  << options.filePaths.size() << " files (" 
                  << duration.count() << " ms)" << std::endl;
    }
    
    return matchCount > 0 ? 0 : 1;  // Return 0 if matches found, 1 otherwise (grep convention)
}