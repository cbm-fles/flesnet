#include <iostream>
#include <string>
#include <vector>

#include "utils.hpp"

std::string compute_common_prefix(const std::vector<std::string>& strings) {
    // TODO: Implement the following less hacky:
    std::string prefix; // default constructor initializes to empty string
    if (strings.size() > 0) {
      prefix = strings[0];
    } else {
      return prefix;
    }
    for (const auto& input : strings) {
      for (unsigned i = 0; i < prefix.size(); ++i) {
        if (i >= input.size()) {
          prefix = prefix.substr(0, i);
          break;
        }
        if (prefix[i] != input[i]) {
          prefix = prefix.substr(0, i);
          break;
        }
      }
      if (prefix.size() == 0) {
        break;
      }
    }
    return prefix;
}

void clean_up_path(std::string& path) {
    // Check if linux:
#ifdef __linux__
    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
      // Add 1 to remove the slash itself:
      path = path.substr(last_slash + 1);
    }
    if (path.size() == 0) {
      // No common prefix found, set arbitrary prefix:
      path = "some_prefix";
    }

#else  // __linux__
    std::cerr << "Error: Using the common prefix of input files as the"
              << std::endl
              << "       prefix for the output files is only implemented"
              << std::endl
              << "       for linux systems. Using arbitrary prefix instead."
              << std::endl;
    path = "some_prefix";
#endif // __linux__
}
