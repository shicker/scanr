#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <memory>
#include <getopt.h>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;

// Configuration structure
struct ScanrConfig {
    bool case_insensitive = false;
    bool invert_match = false;
    bool recursive = false;
    bool show_line_numbers = false;
    bool show_filename_only = false;
    bool count_only = false;
    bool quiet = false;
    bool color_output = true;
    bool whole_word = false;
    bool whole_line = false;
    int max_threads = std::thread::hardware_concurrency();
    std::string pattern;
    std::vector<std::string> files_and_dirs;
};

// ANSI color codes
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string BOLD = "\033[1m";
}

// Thread-safe queue for file processing
template<typename T>
class ConcurrentQueue {
private:
    std::queue<T> queue;
    mutable std::mutex mutex;
    std::condition_variable condition;
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(value));
        condition.notify_one();
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) {
            return false;
        }
        value = std::move(queue.front());
        queue.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
};

// Global variables for thread synchronization
std::mutex output_mutex;
std::atomic<int> total_matches(0);
std::atomic<int> files_processed(0);

// Function to compile regex based on configuration
std::regex compile_regex(const ScanrConfig& config) {
    std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;

    if (config.case_insensitive) {
        flags |= std::regex_constants::icase;
    }

    std::string processed_pattern = config.pattern;
    if (config.whole_word) {
        processed_pattern = "\\b" + processed_pattern + "\\b";
    }
    if (config.whole_line) {
        processed_pattern = "^" + processed_pattern + "$";
    }

    try {
        return std::regex(processed_pattern, flags);
    } catch (const std::regex_error& e) {
        std::cerr << "Invalid regular expression: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Process a single file
void process_file(const fs::path& file_path, const ScanrConfig& config, const std::regex& regex) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cerr << "scanr: " << file_path << ": Unable to open file" << std::endl;
        return;
    }

    std::string line;
    int line_number = 0;
    int match_count = 0;
    std::string filename = file_path.filename().string();
    std::string full_path = file_path.string();

    while (std::getline(file, line)) {
        line_number++;
        bool match = std::regex_search(line, regex);

        if (match != config.invert_match) {
            match_count++;
            total_matches++;

            if (config.count_only || config.quiet || config.show_filename_only) {
                continue;
            }

            std::lock_guard<std::mutex> lock(output_mutex);
            
            if (config.color_output) {
                // Highlight matches in the line
                std::smatch sm;
                std::string::const_iterator start = line.cbegin();
                std::string highlighted_line;

                while (std::regex_search(start, line.cend(), sm, regex)) {
                    highlighted_line += std::string(start, sm[0].first);
                    highlighted_line += Colors::RED + sm[0].str() + Colors::RESET;
                    start = sm[0].second;
                }
                highlighted_line += std::string(start, line.cend());

                if (config.show_line_numbers) {
                    std::cout << Colors::BLUE << full_path << Colors::RESET << ":" 
                              << Colors::GREEN << line_number << Colors::RESET << ":" 
                              << highlighted_line << std::endl;
                } else {
                    std::cout << Colors::BLUE << full_path << Colors::RESET << ":" 
                              << highlighted_line << std::endl;
                }
            } else {
                if (config.show_line_numbers) {
                    std::cout << full_path << ":" << line_number << ":" << line << std::endl;
                } else {
                    std::cout << full_path << ":" << line << std::endl;
                }
            }
        }
    }

    files_processed++;

    std::lock_guard<std::mutex> lock(output_mutex);
    if (config.show_filename_only && match_count > 0) {
        std::cout << full_path << std::endl;
    } else if (config.count_only) {
        std::cout << full_path << ":" << match_count << std::endl;
    }
}

// Worker thread function
void worker_thread(ConcurrentQueue<fs::path>& file_queue, const ScanrConfig& config, const std::regex& regex) {
    fs::path file_path;
    while (file_queue.try_pop(file_path)) {
        process_file(file_path, config, regex);
    }
}

// Recursively collect files from directory
void collect_files(const fs::path& path, ConcurrentQueue<fs::path>& file_queue, const ScanrConfig& config) {
    try {
        if (fs::is_directory(path)) {
            if (!config.recursive) {
                std::cerr << "scanr: " << path << ": Is a directory (use -r to search recursively)" << std::endl;
                return;
            }

            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    file_queue.push(entry.path());
                }
            }
        } else if (fs::is_regular_file(path)) {
            file_queue.push(path);
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "scanr: " << e.path1() << ": " << e.what() << std::endl;
    }
}

// Parse command line arguments
ScanrConfig parse_arguments(int argc, char* argv[]) {
    ScanrConfig config;
    int opt;

    static struct option long_options[] = {
        {"ignore-case", no_argument, nullptr, 'i'},
        {"invert-match", no_argument, nullptr, 'v'},
        {"recursive", no_argument, nullptr, 'r'},
        {"line-number", no_argument, nullptr, 'n'},
        {"files-with-matches", no_argument, nullptr, 'l'},
        {"count", no_argument, nullptr, 'c'},
        {"quiet", no_argument, nullptr, 'q'},
        {"no-color", no_argument, nullptr, 'C'},
        {"word-regexp", no_argument, nullptr, 'w'},
        {"line-regexp", no_argument, nullptr, 'x'},
        {"threads", required_argument, nullptr, 'j'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    while ((opt = getopt_long(argc, argv, "ivrnlcqCwxj:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'i': config.case_insensitive = true; break;
            case 'v': config.invert_match = true; break;
            case 'r': config.recursive = true; break;
            case 'n': config.show_line_numbers = true; break;
            case 'l': config.show_filename_only = true; break;
            case 'c': config.count_only = true; break;
            case 'q': config.quiet = true; break;
            case 'C': config.color_output = false; break;
            case 'w': config.whole_word = true; break;
            case 'x': config.whole_line = true; break;
            case 'j': 
                try {
                    config.max_threads = std::stoi(optarg);
                    if (config.max_threads < 1) {
                        throw std::invalid_argument("Thread count must be positive");
                    }
                } catch (...) {
                    std::cerr << "Invalid thread count: " << optarg << std::endl;
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                std::cout << "Usage: scanr [OPTIONS] PATTERN [FILE...]\n"
                          << "Search for PATTERN in each FILE or standard input.\n"
                          << "Example: scanr -i 'hello world' *.txt\n\n"
                          << "Options:\n"
                          << "  -i, --ignore-case       ignore case distinctions\n"
                          << "  -v, --invert-match      select non-matching lines\n"
                          << "  -r, --recursive         search directories recursively\n"
                          << "  -n, --line-number       print line number with output\n"
                          << "  -l, --files-with-matches  print only names of matching files\n"
                          << "  -c, --count             print only a count of matching lines\n"
                          << "  -q, --quiet             suppress all normal output\n"
                          << "  -C, --no-color          disable color output\n"
                          << "  -w, --word-regexp       force PATTERN to match only whole words\n"
                          << "  -x, --line-regexp       force PATTERN to match only whole lines\n"
                          << "  -j, --threads=NUM       use NUM worker threads (default: CPU count)\n"
                          << "  -h, --help              display this help and exit\n";
                exit(EXIT_SUCCESS);
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        std::cerr << "scanr: no pattern provided\n"
                  << "Try 'scanr --help' for more information." << std::endl;
        exit(EXIT_FAILURE);
    }

    config.pattern = argv[optind++];

    while (optind < argc) {
        config.files_and_dirs.emplace_back(argv[optind++]);
    }

    if (config.files_and_dirs.empty()) {
        std::cerr << "scanr: no input files specified\n"
                  << "Try 'scanr --help' for more information." << std::endl;
        exit(EXIT_FAILURE);
    }

    if (config.quiet) {
        config.show_filename_only = false;
        config.count_only = false;
        config.show_line_numbers = false;
    }

    return config;
}

int main(int argc, char* argv[]) {
    ScanrConfig config = parse_arguments(argc, argv);
    std::regex regex = compile_regex(config);

    ConcurrentQueue<fs::path> file_queue;

    // Collect files to process
    for (const auto& path_str : config.files_and_dirs) {
        collect_files(path_str, file_queue, config);
    }

    if (file_queue.empty()) {
        if (!config.quiet) {
            std::cerr << "scanr: no valid files to process" << std::endl;
        }
        return EXIT_FAILURE;
    }

    // Create worker threads
    std::vector<std::thread> workers;
    int actual_threads = std::min(config.max_threads, static_cast<int>(file_queue.size()));
    
    for (int i = 0; i < actual_threads; ++i) {
        workers.emplace_back(worker_thread, std::ref(file_queue), std::ref(config), std::ref(regex));
    }

    // Wait for all workers to finish
    for (auto& worker : workers) {
        worker.join();
    }

    if (!config.quiet && !config.show_filename_only && !config.count_only) {
        std::cout << "\nTotal matches found: " << total_matches 
                  << " in " << files_processed << " files" << std::endl;
    }

    return total_matches > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}