// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cassert>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <vector>

/// Overloaded output operator for STL vectors.
template <class T>
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
  copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
  return os;
}

inline std::string human_readable_count(uint64_t bytes,
                                        bool use_si = false,
                                        const std::string& unit_string = "B") {
  uint64_t unit = use_si ? 1000 : 1024;

  if (bytes < unit) {
    return std::to_string(bytes) + " " + unit_string;
  }

  uint32_t exponent = static_cast<uint32_t>(std::log(bytes) / std::log(unit));

  std::string prefix =
      std::string(use_si ? "kMGTPE" : "KMGTPE").substr(exponent - 1, 1);
  if (!use_si) {
    prefix += "i";
  }
  std::stringstream st;
  st.precision(4);
  st << (bytes / std::pow(unit, exponent)) << " " << prefix << unit_string;

  return st.str();
}

template <typename T>
inline std::string
bar_graph(std::vector<T> values, std::string symbols, uint32_t length) {
  assert(!symbols.empty());

  std::string s;
  s.reserve(length);

  T sum = 0;
  for (T n : values) {
    assert(n >= 0);
    sum += n;
  }

  float filled = 0.0;
  for (size_t i = 0; i < values.size(); ++i) {
    filled += static_cast<float>(values[i]) / static_cast<float>(sum);
    uint32_t chars =
        static_cast<uint32_t>(std::round(filled * static_cast<float>(length)));
    s.append(std::string(chars - s.size(), symbols[i % symbols.size()]));
  }

  return s;
}

inline double calculate_ber(size_t n_bytes, size_t n_errors) {
  // calculates upper limit BER with 95% confidence level
  // for up to 6 error
  static constexpr std::array<float, 7> coeff{
      {2.996, 4.744, 6.296, 7.754, 9.154, 10.51, 11.84}};
  double ber = 0;
  if (n_errors <= 6) {
    ber = coeff.at(n_errors) / (n_bytes * 8.0);
  }
  return ber;
}

/// Missing library function
inline unsigned stou(std::string const& str, size_t* idx = 0, int base = 10) {
  unsigned long result = std::stoul(str, idx, base);
  if (result > std::numeric_limits<unsigned>::max()) {
    throw std::out_of_range("stou");
  }
  return static_cast<unsigned>(result);
}
