#include <iostream>        // For standard I/O (cout, cerr, cin)
#include <fstream>         // For file I/O (ifstream)
#include <string>          // For string manipulation
#include <vector>          // For storing patterns, files, matches
#include <regex>           // For regular expression support (-E, -w, -i, -o)
#include <stdexcept>       // For standard exceptions
#include <deque>           // For efficient handling of context lines (-A, -B, -C)
#include <set>             // Could be used for tracking printed lines (alternative to last_printed_line)
#include <algorithm>       // For std::transform, std::sort, std::search
#include <cctype>          // For tolower, isalnum

// Structure to hold the parsed command-line options and settings
struct Settings {
    bool count_only = false;         // -c: Print only a count of matching lines
    bool hide_filenames = false;     // -h: Suppress filename prefixes
    bool ignore_case = false;        // -i: Ignore case distinctions
    bool list_filenames = false;     // -l: Print only names of files with matches
    bool show_line_numbers = false;  // -n: Prefix output lines with line numbers
    bool invert_match = false;       // -v: Select non-matching lines
    bool use_extended_regex = false; // -E: Treat pattern as an Extended Regular Expression
    bool match_whole_word = false;   // -w: Match only whole words
    bool only_matching = false;      // -o: Print only the matched parts of lines
    int lines_after = 0;             // -A n: Print n lines of trailing context
    int lines_before = 0;            // -B n: Print n lines of leading context
    std::vector<std::string> patterns; // List of patterns to search for
    std::vector<std::string> files;    // List of input files to process
};

// --- Function Prototypes ---
void print_usage();
bool parse_arguments(int argc, char* argv[], Settings& settings);
void process_stream(std::istream& input, const std::string& filename, const Settings& settings, bool show_filename_prefix);
std::vector<std::regex> compile_patterns(const Settings& settings);
bool regex_matches(const std::string& line, const std::vector<std::regex>& regex_patterns, const Settings& settings, std::vector<std::pair<size_t, size_t>>& match_positions);
bool simple_matches(const std::string& line, const Settings& settings, std::vector<std::pair<size_t, size_t>>& match_positions);
bool is_word_boundary(const std::string& line, size_t pos);
size_t find_insensitive(const std::string& haystack, const std::string& needle, size_t pos);


// --- Main Function ---
int main(int argc, char* argv[]) {
    Settings settings;

    // 1. Parse Command Line Arguments
    if (!parse_arguments(argc, argv, settings)) {
        return 1; // Exit if parsing failed
    }

    // Ensure at least one pattern was provided
    if (settings.patterns.empty()) {
        std::cerr << "scanr: No pattern provided." << std::endl;
        print_usage();
        return 1;
    }

    // Determine if regex engine is needed (simplifies -i, -w, -o)
    if (settings.match_whole_word || settings.ignore_case || settings.only_matching) {
         settings.use_extended_regex = true; // Force regex use for simplicity/correctness
    }

    // Pre-compile regex patterns if regex will be used
    std::vector<std::regex> regex_patterns;
    if (settings.use_extended_regex) {
        try {
            regex_patterns = compile_patterns(settings);
        } catch (const std::regex_error& e) {
            std::cerr << "scanr: Invalid regular expression: " << e.what() << " (Pattern: " << e.what() /* Might not show pattern */ << ")" << std::endl;
            return 1;
        }
         catch (const std::exception& e) {
             std::cerr << "scanr: Error compiling regex: " << e.what() << std::endl;
             return 1;
         }
    }


    // Determine if filename prefix should be shown (multiple files and not disabled)
    bool show_filename_prefix = settings.files.size() > 1 && !settings.hide_filenames && !settings.list_filenames && !settings.count_only;

    // 2. Process Input (Standard Input or Files)
    if (settings.files.empty()) {
        // Process standard input
        process_stream(std::cin, "(standard input)", settings, false); // No prefix for stdin
    } else {
        // Process each file provided
        for (const auto& filename : settings.files) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                // Report error but continue with other files unless in modes where errors aren't useful
                 if (!settings.list_filenames && !settings.count_only) {
                    std::cerr << "scanr: Cannot open file '" << filename << "'" << std::endl;
                 }
                // Consider returning an error code if *any* file fails? Standard grep usually doesn't.
                continue; // Skip to the next file
            }
            process_stream(file, filename, settings, show_filename_prefix);
            file.close(); // Good practice to close the file
        }
    }

    return 0; // Success
}

// --- Helper Functions ---

// Print usage instructions to standard error
void print_usage() {
    std::cerr << "Usage: scanr [OPTIONS]... PATTERN [FILE]...\n"
              << "   or: scanr [OPTIONS]... -e PATTERN... [FILE]...\n"
              << "   or: scanr [OPTIONS]... -f FILE_WITH_PATTERNS [FILE]...\n"
              << "Search for PATTERN in each FILE or standard input.\n\n"
              << "Options:\n"
              << "  -c, --count            Print only a count of matching lines per file\n"
              << "  -h                     Suppress the prefixing of filenames on output\n"
              << "  -i                     Ignore case distinctions\n"
              << "  -l                     Print only names of files containing matches\n"
              << "  -n                     Prefix each line of output with the line number\n"
              << "  -v                     Select non-matching lines\n"
              << "  -e PATTERN             Use PATTERN for matching (can be used multiple times)\n"
              << "  -f FILE                Obtain patterns from FILE, one per line\n"
              << "  -E                     Interpret PATTERN as an extended regular expression (ERE)\n"
              << "  -w                     Match only whole words (forces -E)\n"
              << "  -o                     Print only the matched parts of lines (forces -E)\n"
              << "  -A NUM                 Print NUM lines of trailing context\n"
              << "  -B NUM                 Print NUM lines of leading context\n"
              << "  -C NUM                 Print NUM lines of output context (equivalent to -A NUM -B NUM)\n"
              << std::endl;
}

// Parse command-line arguments and populate the Settings struct
bool parse_arguments(int argc, char* argv[], Settings& settings) {
    std::vector<std::string> pattern_sources; // Temp store for patterns from -e or command line arg

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--") { // Treat all subsequent arguments as files
            while (++i < argc) {
                settings.files.push_back(argv[i]);
            }
            break;
        }

        if (arg.length() > 1 && arg[0] == '-') {
            // Option handling
            if (arg == "-c" || arg == "--count") {
                settings.count_only = true;
            } else if (arg == "-h") {
                settings.hide_filenames = true;
            } else if (arg == "-i") {
                settings.ignore_case = true;
            } else if (arg == "-l") {
                settings.list_filenames = true;
            } else if (arg == "-n") {
                settings.show_line_numbers = true;
            } else if (arg == "-v") {
                settings.invert_match = true;
            } else if (arg == "-E") {
                settings.use_extended_regex = true;
            } else if (arg == "-w") {
                settings.match_whole_word = true;
            } else if (arg == "-o") {
                settings.only_matching = true;
            } else if (arg == "-e") {
                if (++i < argc) {
                    pattern_sources.push_back(argv[i]);
                } else {
                    std::cerr << "scanr: Option '-e' requires a pattern argument." << std::endl;
                    return false;
                }
            } else if (arg == "-f") {
                if (++i < argc) {
                    std::ifstream pattern_file(argv[i]);
                    if (!pattern_file.is_open()) {
                        std::cerr << "scanr: Cannot open pattern file '" << argv[i] << "'" << std::endl;
                        return false;
                    }
                    std::string line;
                    bool added_pattern = false;
                    while (std::getline(pattern_file, line)) {
                        // Remove potential trailing newline characters (\r\n or \n)
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                         if (!line.empty()) {
                            settings.patterns.push_back(line);
                            added_pattern = true;
                         }
                    }
                    pattern_file.close();
                    if (!added_pattern && pattern_sources.empty() && settings.patterns.empty()) {
                         std::cerr << "scanr: Warning: Pattern file '" << argv[i] << "' is empty or contains no valid patterns." << std::endl;
                         // Continue, maybe other patterns were provided
                    }
                } else {
                    std::cerr << "scanr: Option '-f' requires a filename argument." << std::endl;
                    return false;
                }
            } else if (arg == "-A") {
                 if (++i < argc) {
                    try {
                        settings.lines_after = std::stoi(argv[i]);
                        if (settings.lines_after < 0) throw std::invalid_argument("Negative value");
                    } catch (const std::exception& e) {
                        std::cerr << "scanr: Invalid non-negative integer for option '-A': '" << argv[i] << "'" << std::endl;
                        return false;
                    }
                } else {
                    std::cerr << "scanr: Option '-A' requires a non-negative integer argument." << std::endl;
                    return false;
                }
            } else if (arg == "-B") {
                 if (++i < argc) {
                    try {
                        settings.lines_before = std::stoi(argv[i]);
                         if (settings.lines_before < 0) throw std::invalid_argument("Negative value");
                    } catch (const std::exception& e) {
                        std::cerr << "scanr: Invalid non-negative integer for option '-B': '" << argv[i] << "'" << std::endl;
                        return false;
                    }
                } else {
                    std::cerr << "scanr: Option '-B' requires a non-negative integer argument." << std::endl;
                    return false;
                }
            } else if (arg == "-C") {
                 if (++i < argc) {
                    try {
                        int context = std::stoi(argv[i]);
                        if (context < 0) throw std::invalid_argument("Negative value");
                        settings.lines_after = context;
                        settings.lines_before = context;
                    } catch (const std::exception& e) {
                        std::cerr << "scanr: Invalid non-negative integer for option '-C': '" << argv[i] << "'" << std::endl;
                        return false;
                    }
                } else {
                    std::cerr << "scanr: Option '-C' requires a non-negative integer argument." << std::endl;
                    return false;
                }
            } else {
                // Handle combined short options like -inv
                for (size_t j = 1; j < arg.length(); ++j) {
                    switch (arg[j]) {
                        case 'c': settings.count_only = true; break;
                        case 'h': settings.hide_filenames = true; break;
                        case 'i': settings.ignore_case = true; break;
                        case 'l': settings.list_filenames = true; break;
                        case 'n': settings.show_line_numbers = true; break;
                        case 'v': settings.invert_match = true; break;
                        case 'E': settings.use_extended_regex = true; break;
                        case 'w': settings.match_whole_word = true; break;
                        case 'o': settings.only_matching = true; break;
                        default:
                            std::cerr << "scanr: Invalid option -- '" << arg[j] << "' in '" << arg << "'" << std::endl;
                            print_usage();
                            return false;
                    }
                }
            }
        } else {
            // Not an option, treat as pattern or filename
            if (pattern_sources.empty() && settings.patterns.empty()) {
                // If no patterns loaded from -f or -e yet, this is the main pattern
                pattern_sources.push_back(arg);
            } else {
                // Otherwise, it's an input file
                settings.files.push_back(arg);
            }
        }
    }

    // Add patterns from -e / command line argument to the main list
    settings.patterns.insert(settings.patterns.end(), pattern_sources.begin(), pattern_sources.end());

    // Sanity checks and adjustments for conflicting options
    if (settings.list_filenames || settings.count_only) {
        // Context options don't make sense with -l or -c
        settings.lines_after = 0;
        settings.lines_before = 0;
        settings.only_matching = false; // -o also doesn't make sense
    }
    if (settings.only_matching) {
         // Context options usually ignored with -o in standard grep
        settings.lines_after = 0;
        settings.lines_before = 0;
    }


    return true; // Parsing successful
}

// Compile string patterns into std::regex objects
std::vector<std::regex> compile_patterns(const Settings& settings) {
    std::vector<std::regex> regex_patterns;
    // Set regex syntax options (ERE is default for std::regex)
    auto flags = std::regex::ECMAScript; // Default syntax, generally compatible with ERE
    // Or use std::regex::extended if available and preferred, but ECMAScript is more common now
    // auto flags = std::regex::extended;

    if (settings.ignore_case) {
        flags |= std::regex::icase;
    }

    for (const auto& p_str : settings.patterns) {
        std::string final_pattern = p_str;
        if (settings.match_whole_word) {
            // Wrap with word boundaries (\b).
            // Note: This is a basic implementation. More complex patterns might interact
            // unexpectedly with automatically added boundaries.
            // Check if pattern already starts/ends with word boundary or anchor?
            bool starts_with_boundary = (final_pattern.length() >= 2 && final_pattern.substr(0, 2) == "\\b") || (final_pattern.length() >= 1 && final_pattern[0] == '^');
            bool ends_with_boundary = (final_pattern.length() >= 2 && final_pattern.substr(final_pattern.length() - 2) == "\\b") || (final_pattern.length() >= 1 && final_pattern.back() == '$');

            if (!starts_with_boundary) {
                 final_pattern.insert(0, "\\b");
            }
             if (!ends_with_boundary) {
                 final_pattern.append("\\b");
             }
        }
        // Add the compiled regex to the list
        regex_patterns.emplace_back(final_pattern, flags);
    }
    return regex_patterns;
}


// Check if a line matches any pattern using REGEX
bool regex_matches(const std::string& line, const std::vector<std::regex>& regex_patterns, const Settings& settings, std::vector<std::pair<size_t, size_t>>& match_positions) {
     match_positions.clear();
     bool found_match = false;
     std::smatch match_result; // Stores details of a single match

    for (const auto& pattern_regex : regex_patterns) {
         if (settings.only_matching) {
             // Find *all* non-overlapping matches using an iterator
             auto words_begin = std::sregex_iterator(line.begin(), line.end(), pattern_regex);
             auto words_end = std::sregex_iterator();

             for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                 std::smatch match = *i;
                 // Store start position and length of the matched substring
                 match_positions.push_back({static_cast<size_t>(match.position(0)), static_cast<size_t>(match.length(0))});
                 found_match = true;
             }
             // Continue checking other patterns even if matches found for this one in -o mode
         } else {
             // Find if *any* match exists using regex_search
             if (std::regex_search(line, match_result, pattern_regex)) {
                 found_match = true;
                 // Store the first match found (though not strictly needed if not -o)
                 match_positions.push_back({static_cast<size_t>(match_result.position(0)), static_cast<size_t>(match_result.length(0))});
                 break; // Found a match with one pattern, no need to check others unless -o
             }
         }
    }
     // Sort matches by start position if -o is active, for ordered output
     if (settings.only_matching && !match_positions.empty()) {
         std::sort(match_positions.begin(), match_positions.end());
     }

    return found_match;
}

// Check if a character position represents a word boundary (for simple_matches -w)
// True if transition between alphanumeric and non-alphanumeric, or at start/end.
bool is_word_boundary(const std::string& line, size_t pos) {
    if (line.empty()) return true; // Empty line has boundaries everywhere?

    bool pos_is_alnum = (pos < line.length()) && std::isalnum(static_cast<unsigned char>(line[pos]));

    if (pos == 0) {
        return pos_is_alnum; // Boundary if first char is alphanumeric
    } else if (pos == line.length()) {
        bool prev_is_alnum = std::isalnum(static_cast<unsigned char>(line[pos - 1]));
        return prev_is_alnum; // Boundary if last char was alphanumeric
    } else {
        bool prev_is_alnum = std::isalnum(static_cast<unsigned char>(line[pos - 1]));
        // Boundary if one side is alphanumeric and the other is not
        return prev_is_alnum != pos_is_alnum;
    }
}


// Case-insensitive string search helper
size_t find_insensitive(const std::string& haystack, const std::string& needle, size_t pos = 0) {
    if (needle.empty()) return pos; // Match empty needle immediately
    auto it = std::search(
        haystack.begin() + pos, haystack.end(), // Range to search in
        needle.begin(), needle.end(),           // Range to search for
        [](unsigned char ch1, unsigned char ch2) { return std::tolower(ch1) == std::tolower(ch2); } // Comparison predicate
    );
    return (it == haystack.end()) ? std::string::npos : std::distance(haystack.begin(), it);
}


// Check if a line matches any pattern using SIMPLE string search (no regex)
bool simple_matches(const std::string& line, const Settings& settings, std::vector<std::pair<size_t, size_t>>& match_positions) {
    match_positions.clear();
    bool found_match_overall = false;

    for (const auto& pattern : settings.patterns) {
        if (pattern.empty()) continue; // Skip empty patterns

        size_t current_pos = 0;
        bool found_match_this_pattern = false;

        while (current_pos < line.length()) {
             size_t match_start;
             // Find next occurrence of the pattern
             if (settings.ignore_case) {
                 match_start = find_insensitive(line, pattern, current_pos);
             } else {
                 match_start = line.find(pattern, current_pos);
             }

            if (match_start == std::string::npos) {
                break; // No more occurrences of this pattern found
            }

            // Check whole word condition if -w is set
             bool word_match_ok = true;
             if (settings.match_whole_word) {
                 // Check boundary before the match and after the match
                 bool start_boundary_ok = is_word_boundary(line, match_start);
                 bool end_boundary_ok = is_word_boundary(line, match_start + pattern.length());
                 word_match_ok = start_boundary_ok && end_boundary_ok;
             }

             if (word_match_ok) {
                 // Match is valid (considering -w)
                 found_match_overall = true;
                 found_match_this_pattern = true;
                 match_positions.push_back({match_start, pattern.length()});

                 if (!settings.only_matching) {
                     // If not -o, finding one match for this pattern is enough
                     goto next_pattern; // Exit inner loop and check next pattern
                 }
                 // If -o, continue searching for more occurrences of *this* pattern
                 // Advance position to search after the start of the current match
                 // (to handle overlapping matches correctly if needed, though simple find won't overlap itself)
                 current_pos = match_start + 1; // Or match_start + pattern.length() if non-overlapping needed

             } else {
                 // Word boundary check failed, advance search position past this potential match
                 current_pos = match_start + 1;
             }
        } // End while searching for current pattern

        next_pattern:; // Label for goto
         if (found_match_overall && !settings.only_matching) {
             break; // Found a match with one pattern, stop checking others if not -o
         }

    } // End for each pattern

     // Sort matches by start position if -o is active
     if (settings.only_matching && !match_positions.empty()) {
         std::sort(match_positions.begin(), match_positions.end());
     }

    return found_match_overall;
}


// Process a single input stream (file or stdin)
void process_stream(std::istream& input, const std::string& filename, const Settings& settings, bool show_filename_prefix) {
    std::string line;
    long long line_number = 0;
    long long match_count = 0;
    bool filename_printed_for_l = false; // Track if filename printed for -l mode

    // --- Context Handling Variables ---
    // Buffer to store recent lines for -B context
    std::deque<std::pair<long long, std::string>> before_buffer;
    // Counter for how many lines to print *after* the current match for -A context
    int after_lines_to_print = 0;
    // Track the line number of the last line printed to manage context overlaps/separators
    long long last_printed_line = -1;
    // Flag to indicate if a "--" separator needs to be printed before the next output line
    bool pending_separator = false;

    // Re-compile patterns here if needed (passed as arg would be better)
    std::vector<std::regex> regex_patterns;
     if (settings.use_extended_regex) {
         try {
             regex_patterns = compile_patterns(settings);
         } catch (const std::regex_error& e) {
             std::cerr << "scanr: [" << filename << "] Invalid regex: " << e.what() << std::endl;
             return; // Cannot process this stream
         }
          catch (const std::exception& e) {
             std::cerr << "scanr: [" << filename << "] Error compiling regex: " << e.what() << std::endl;
             return;
         }
     }

    // --- Main Line Processing Loop ---
    while (std::getline(input, line)) {
        line_number++;
        std::vector<std::pair<size_t, size_t>> match_positions; // Stores {start_pos, length} for -o
        bool is_match;

        // Perform the matching based on whether regex is used
        if (settings.use_extended_regex) {
            is_match = regex_matches(line, regex_patterns, settings, match_positions);
        } else {
            is_match = simple_matches(line, settings, match_positions);
        }

        // Determine if this line should be printed based on match status and -v (invert)
        bool output_this_line = (is_match != settings.invert_match);

        // --- Output Logic ---

        if (output_this_line) {
            // This line is considered a match for output purposes
            match_count++;

            // Handle -l (list filenames): print filename once and stop processing this file
            if (settings.list_filenames) {
                if (!filename_printed_for_l) {
                    std::cout << filename << std::endl;
                    filename_printed_for_l = true;
                }
                // Optimization: Stop reading this file now. Remove 'return' to match GNU grep exactly (reads whole file).
                 return;
            }

            // Handle -c (count only): increment count and continue to next line
            if (settings.count_only) {
                continue;
            }

            // --- Context and Regular Output ---

            // Determine if a separator is needed (gap since last printed line)
             if ((settings.lines_before > 0 || settings.lines_after > 0) && last_printed_line != -1 && line_number > last_printed_line + 1) {
                 pending_separator = true; // Mark that a separator is needed before the next output
             }

            // 1. Print Leading Context (-B, -C)
            if (settings.lines_before > 0) {
                 for (const auto& buffered_line_pair : before_buffer) {
                     // Only print buffered lines that haven't already been printed
                     if (buffered_line_pair.first > last_printed_line) {
                          if (pending_separator) {
                             std::cout << "--" << std::endl;
                             pending_separator = false; // Separator printed
                          }
                         // Print the buffered line with appropriate prefixes
                         if (show_filename_prefix) std::cout << filename << "-"; // Use '-' separator for context
                         if (settings.show_line_numbers) std::cout << buffered_line_pair.first << "-"; // Use '-' separator for context
                         std::cout << buffered_line_pair.second << std::endl;
                         last_printed_line = buffered_line_pair.first; // Update last printed line number
                     }
                 }
                 // Clear the buffer? No, keep it sliding.
            }

            // 2. Print the Matching Line (or parts for -o)
             if (line_number > last_printed_line) { // Ensure the matching line itself wasn't printed as context
                 if (pending_separator) {
                     std::cout << "--" << std::endl;
                     pending_separator = false;
                 }

                 // Choose output format based on -o
                 if (settings.only_matching) {
                     // Print only the matched parts, each on a new line
                     for (const auto& match_pos : match_positions) {
                         if (show_filename_prefix) std::cout << filename << ":";
                         if (settings.show_line_numbers) std::cout << line_number << ":";
                         std::cout << line.substr(match_pos.first, match_pos.second) << std::endl;
                     }
                 } else {
                     // Print the whole line
                     if (show_filename_prefix) std::cout << filename << ":"; // Use ':' separator for match line
                     if (settings.show_line_numbers) std::cout << line_number << ":"; // Use ':' separator for match line
                     std::cout << line << std::endl;
                 }
                 last_printed_line = line_number; // Update last printed line number
             }


            // 3. Set up Trailing Context (-A, -C)
            // Set the counter for how many subsequent lines need to be printed
            after_lines_to_print = settings.lines_after;

        } else {
            // Line did *not* match (or matched but -v is active)
            // Check if it needs to be printed as part of trailing context (-A, -C)
            if (after_lines_to_print > 0) {
                 if (line_number > last_printed_line) { // Avoid re-printing if already printed
                      if (pending_separator) {
                         // Separator might be needed if the previous block ended,
                         // and this is the first line of trailing context.
                         std::cout << "--" << std::endl;
                         pending_separator = false;
                      }
                     // Print the context line with appropriate prefixes
                     if (show_filename_prefix) std::cout << filename << "-"; // Use '-' separator for context
                     if (settings.show_line_numbers) std::cout << line_number << "-"; // Use '-' separator for context
                     std::cout << line << std::endl;
                     last_printed_line = line_number; // Update last printed line number
                 }
                after_lines_to_print--; // Decrement the counter
            }
        }

        // --- Update Context Buffer ---
        // Add the current line to the 'before' buffer for potential future use
        if (settings.lines_before > 0) {
            before_buffer.push_back({line_number, line});
            // Keep the buffer size limited to lines_before
            if (before_buffer.size() > static_cast<size_t>(settings.lines_before)) {
                before_buffer.pop_front(); // Remove the oldest line
            }
        }
    } // End while getline loop

    // --- Final Output After Processing Stream ---
    // Print the total count if -c was specified
    if (settings.count_only) {
        // Prefix with filename if multiple files were given or if explicitly not hidden
        if (show_filename_prefix || (settings.files.size() == 1 && !settings.hide_filenames) || filename == "(standard input)") {
             std::cout << filename << ":";
        }
        std::cout << match_count << std::endl;
    }
}
