#include <iostream>
#include <string>
#include <vector>

#include "utils.hpp"

std::string compute_common_prefix(const std::vector<std::string>& strings) {
  std::string prefix; // default constructor initializes to empty string
  if (strings.size() == 0) {
    // If there are no strings, return an empty string
    return prefix;
  } 

  // Initialize the prefix to the first string
  prefix = strings[0];

  // Iterate through the strings and truncate the prefix as needed
  for (const auto& input : strings) {

    // If the prefix is empty, there is no common prefix and we can stop
    if (prefix.size() == 0) {
      break;
    }

    if (input.size() < prefix.size()) {
      // If the input is shorter than the prefix, truncate the prefix
      // to the length of the input
      prefix = prefix.substr(0, input.size());
    }

    // Compare the prefix and the input, and truncate the prefix as
    // needed
    for (unsigned i = 0; i < prefix.size(); ++i) {
      if (prefix[i] != input[i]) {
        prefix = prefix.substr(0, i);
        break;
      }
    }

  }

  return prefix;
}
