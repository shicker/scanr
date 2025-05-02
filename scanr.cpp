 #include <iostream>     // For console input/output (std::cout, std::cerr)
 #include <fstream>      // For file stream operations (std::ifstream)
 #include <string>       // For string manipulation (std::string)
 #include <vector>       // For storing arguments (std::vector)
 #include <filesystem>   // For directory traversal (requires C++17)
 #include <stdexcept>    // For standard exceptions
 #include <system_error> // For filesystem errors
 #include <algorithm>    // For std::transform
 #include <cctype>       // For std::tolower
 
 // Function prototypes
 void search_in_file(const std::string& pattern, const std::filesystem::path& file_path, bool case_insensitive, bool& found_match_global);
 void process_path(const std::string& pattern, const std::filesystem::path& path, bool case_insensitive, bool& found_match_global);
 std::string to_lower(const std::string& str); // Helper for case-insensitive search
 
 /**
  * @brief Converts a string to lowercase.
  * @param str The input string.
  * @return The lowercase version of the string.
  */
 std::string to_lower(const std::string& str) {
     std::string lower_str = str;
     std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                    [](unsigned char c){ return std::tolower(c); });
     return lower_str;
 }
 
 /**
  * @brief Searches for a pattern within a single file, line by line.
  * Prints matching lines prefixed with the filename.
  * @param pattern The string pattern to search for.
  * @param file_path The path to the file to search within.
  * @param case_insensitive If true, perform a case-insensitive search.
  * @param found_match_global Reference to a boolean flag, set to true if any match is found in any file.
  */
 void search_in_file(const std::string& pattern, const std::filesystem::path& file_path, bool case_insensitive, bool& found_match_global) {
     // Open the file for reading
     std::ifstream file_stream(file_path);
 
     // Check if the file was opened successfully
     if (!file_stream.is_open()) {
         std::cerr << "scanr: Error opening file: " << file_path.string() << std::endl;
         return; // Skip this file
     }
 
     std::string line;
     int line_number = 0;
     std::string search_pattern = case_insensitive ? to_lower(pattern) : pattern;
 
     // Read the file line by line
     while (std::getline(file_stream, line)) {
         line_number++;
 
         // Prepare the line for comparison based on case sensitivity
         std::string line_to_compare = case_insensitive ? to_lower(line) : line;
 
         // Check if the pattern exists within the current line
         // std::string::npos indicates that the pattern was not found
         if (line_to_compare.find(search_pattern) != std::string::npos) {
             // Print the original filename, line number, and the original matching line
             std::cout << file_path.string() << ":" << line_number << ":" << line << std::endl;
             found_match_global = true; // Mark that at least one match was found overall
         }
     }
     // File stream is automatically closed when file_stream goes out of scope
 }
 
 /**
  * @brief Processes a given path. If it's a file, searches within it.
  * If it's a directory, recursively searches files within it.
  * @param pattern The string pattern to search for.
  * @param path The filesystem path (file or directory) to process.
  * @param case_insensitive If true, perform a case-insensitive search.
  * @param found_match_global Reference to a boolean flag, set to true if any match is found in any file.
  */
 void process_path(const std::string& pattern, const std::filesystem::path& path, bool case_insensitive, bool& found_match_global) {
     try {
         // Check if the path exists
         if (!std::filesystem::exists(path)) {
             std::cerr << "scanr: Error: Path does not exist: " << path.string() << std::endl;
             return;
         }
 
         // Check if the path refers to a regular file
         if (std::filesystem::is_regular_file(path)) {
             search_in_file(pattern, path, case_insensitive, found_match_global);
         }
         // Check if the path refers to a directory
         else if (std::filesystem::is_directory(path)) {
             // Use a recursive directory iterator to traverse the directory
             // and all its subdirectories.
             std::error_code ec; // To check for errors during iteration
             for (const auto& entry : std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied, ec)) {
                 // Check if the current entry is a regular file (and handle potential errors)
                  if (!ec && entry.is_regular_file(ec) && !ec) {
                     search_in_file(pattern, entry.path(), case_insensitive, found_match_global);
                  }
                  // Log errors encountered during file type check, but continue
                  if(ec) {
                     std::cerr << "scanr: Warning: Error accessing entry " << entry.path().string() << ": " << ec.message() << std::endl;
                     ec.clear(); // Clear the error to allow iteration to continue
                  }
             }
              // Log errors encountered during directory iteration itself (e.g., top-level permission issue)
              if(ec) {
                  std::cerr << "scanr: Warning: Error iterating directory " << path.string() << ": " << ec.message() << std::endl;
              }
         } else {
             // Handle cases where the path is neither a file nor a directory
             std::cerr << "scanr: Error: Path is not a regular file or directory: " << path.string() << std::endl;
         }
     } catch (const std::filesystem::filesystem_error& e) {
         // Catch potential exceptions from filesystem operations
         std::cerr << "scanr: Filesystem error processing path " << path.string() << ": " << e.what() << std::endl;
     } catch (const std::exception& e) {
         // Catch other standard exceptions
         std::cerr << "scanr: An unexpected error occurred: " << e.what() << std::endl;
     }
 }
 
 
 /**
  * @brief Main entry point of the program. Parses arguments and initiates search.
  * @param argc Argument count.
  * @param argv Argument vector.
  * @return 0 on success, 1 on error (e.g., incorrect arguments).
  */
 int main(int argc, char* argv[]) {
     bool case_insensitive = false;
     std::string pattern;
     std::filesystem::path search_path;
     int arg_index = 1; // Start checking arguments from index 1
 
     // --- Argument Parsing ---
     if (argc < 3 || argc > 4) { // Must have pattern and path, optionally -i
         std::cerr << "Usage: " << argv[0] << " [-i] <pattern> <file_or_directory_path>" << std::endl;
         std::cerr << "Example: " << argv[0] << " -i \"search text\" C:\\MyFolder" << std::endl;
         return 1; // Indicate incorrect usage
     }
 
     // Check for optional -i flag
     if (argc == 4) {
         if (std::string(argv[arg_index]) == "-i") {
             case_insensitive = true;
             arg_index++; // Move to the next argument (pattern)
         } else {
             // If 4 args are given but the first isn't -i, it's an error
              std::cerr << "Usage: " << argv[0] << " [-i] <pattern> <file_or_directory_path>" << std::endl;
              std::cerr << "Error: Invalid option '" << argv[arg_index] << "'" << std::endl;
              return 1;
         }
     }
 
     // Assign pattern and path based on remaining arguments
     pattern = argv[arg_index++];
     search_path = argv[arg_index];
     // --- End Argument Parsing ---
 
 
     bool found_match = false; // Flag to track if any match was found
 
     // Start processing the specified path
     process_path(pattern, search_path, case_insensitive, found_match);
 
     // Check if any matches were found during the entire process
     if (!found_match) {
         std::cerr << "scanr: Pattern not found: \"" << pattern << "\" in " << search_path.string() << std::endl;
         // Returning 1 is common for grep when no lines are selected
         // return 1; // Optional: uncomment for grep-like exit code behavior
     }
 
     return 0; // Indicate successful execution (even if no matches found, unless returning 1 above)
 }
 