// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

/// Overloaded output operator for STL vectors.
template <class T>
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
  copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
  return os;
}

std::string human_readable_count(uint64_t bytes,
                                 bool use_si = false,
                                 const std::string& unit_string = "B");

template <typename T>
inline std::string
bar_graph(std::vector<T> values, std::string symbols, uint32_t length) {
  assert(!symbols.empty());

  std::string s;
  s.reserve(length);

  T sum = 0;
  for (T n : values) {
    if (!(n >= 0)) {
      return std::string("bar_graph(): Assertion `n >= 0' failed.");
    }
    sum += n;
  }

  float filled = 0.0;
  for (size_t i = 0; i < values.size(); ++i) {
    filled += static_cast<float>(values[i]) / static_cast<float>(sum);
    auto chars =
        static_cast<uint32_t>(std::round(filled * static_cast<float>(length)));
    s.append(std::string(chars - s.size(), symbols[i % symbols.size()]));
  }

  return s;
}

/// Calculates upper-limit bit-error-rate (BER) with 95% confidence level for up
/// to 6 errors
double calculate_ber(size_t n_bytes, size_t n_errors);

/// Missing library function
unsigned stou(std::string const& str, size_t* idx = nullptr, int base = 10);

/// Replace all instances of the specified pattern in the input string
[[nodiscard]] std::string replace_all_copy(const std::string& str,
                                           const std::string& from,
                                           const std::string& to);

void replace_all(std::string& str,
                 const std::string& from,
                 const std::string& to);

/// Split string by any of the separators
std::vector<std::string> split(const std::string& str,
                               const std::string& separators);

/// Straightforward URI parser class
struct UriComponents {
  std::string scheme;
  std::string authority;
  std::string path;
  std::string query;
  std::string fragment;
  std::map<std::string, std::string> query_components;

  UriComponents(const std::string& uri);
};
