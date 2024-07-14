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
