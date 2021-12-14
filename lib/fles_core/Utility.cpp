// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#include "Utility.hpp"

#include <array>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>

std::string human_readable_count(uint64_t bytes,
                                 bool use_si,
                                 const std::string& unit_string) {
  const uint64_t unit = use_si ? 1000 : 1024;

  if (bytes < unit) {
    return std::to_string(bytes) + " " + unit_string;
  }

  const auto exponent = static_cast<uint32_t>(std::log(bytes) / std::log(unit));

  using namespace std::string_literals;
  std::string prefix = (use_si ? "kMGTPE"s : "KMGTPE"s).substr(exponent - 1, 1);
  if (!use_si) {
    prefix += "i";
  }
  std::stringstream st;
  st.precision(4);
  st << (bytes / std::pow(unit, exponent)) << " " << prefix << unit_string;

  return st.str();
}

double calculate_ber(size_t n_bytes, size_t n_errors) {
  // calculates upper limit BER with 95% confidence level
  // for up to 6 error
  static constexpr std::array coeff{2.996, 4.744, 6.296, 7.754,
                                    9.154, 10.51, 11.84};
  double ber = 0;
  if (n_errors < coeff.size()) {
    ber = coeff.at(n_errors) / (n_bytes * 8.0);
  }
  return ber;
}

/// Missing library function
unsigned stou(std::string const& str, size_t* idx, int base) {
  unsigned long result = std::stoul(str, idx, base);
  if (result > std::numeric_limits<unsigned>::max()) {
    throw std::out_of_range("stou");
  }
  return static_cast<unsigned>(result);
}

/// Replace all instances of the specified pattern in the input string
// (see: https://stackoverflow.com/a/29752943/1201694)
[[nodiscard]] std::string replace_all_copy(std::string& str,
                                           const std::string& from,
                                           const std::string& to) {
  std::string new_string;
  new_string.reserve(str.length());

  std::string::size_type last_pos = 0;
  std::string::size_type pos;

  while ((pos = str.find(from, last_pos)) != std::string::npos) {
    new_string.append(str, last_pos, pos - last_pos);
    new_string += to;
    last_pos = pos + from.length();
  }
  new_string.append(str, last_pos, str.length() - last_pos);

  return new_string;
}

void replace_all(std::string& str,
                 const std::string& from,
                 const std::string& to) {
  std::string new_string = replace_all_copy(str, from, to);
  str.swap(new_string);
}

/// Split string by any of the separators
std::vector<std::string> split(const std::string& str,
                               const std::string& separators) {
  std::vector<std::string> new_vector;

  std::string::size_type last_pos = 0;
  std::string::size_type pos;

  while ((pos = str.find_first_of(separators, last_pos)) != std::string::npos) {
    if (pos > last_pos) {
      new_vector.emplace_back(str.substr(last_pos, pos - last_pos));
    }
    last_pos = pos + 1;
  }
  if (last_pos < str.length()) {
    new_vector.emplace_back(str.substr(last_pos));
  }

  return new_vector;
}

/// Straightforward URI parser class
UriComponents::UriComponents(std::string uri) {
  // Regular expression from RFC 3986 "URI Generic Syntax"
  // (see https://www.rfc-editor.org/rfc/rfc3986#page-50)
  static const std::regex uri_regex{
      R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)",
      std::regex::extended};
  std::smatch uri_match;
  if (!std::regex_match(uri, uri_match, uri_regex)) {
    throw(std::runtime_error("malformed uri: " + uri));
  }
  scheme = uri_match[2];
  authority = uri_match[4];
  path = uri_match[5];
  query = uri_match[7];
  fragment = uri_match[9];

  for (auto& qc : split(query, "&")) {
    if (!qc.empty()) {
      auto pos = qc.find('=');
      if (pos != std::string::npos) {
        query_components[qc.substr(0, pos)] = qc.substr(pos + 1);
      } else {
        query_components[qc] = "";
      }
    }
  }
}