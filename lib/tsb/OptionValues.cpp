// Copyright 2025 Jan de Cuveland

#include "OptionValues.hpp"
#include <boost/program_options.hpp>
#include <regex>

std::istream& operator>>(std::istream& in, Nanoseconds& time) {
  std::string token;
  in >> token;

  // Parse the time with a suffix
  std::regex r("(-?[0-9]+)(ns|us|µs|ms|s)");
  std::smatch match;

  if (std::regex_match(token, match, r)) {
    long long value = std::stoll(match[1]);
    if (match[2] == "ns") {
      time = Nanoseconds(std::chrono::nanoseconds(value));
    } else if (match[2] == "us" || match[2] == "µs") {
      time = Nanoseconds(std::chrono::microseconds(value));
    } else if (match[2] == "ms") {
      time = Nanoseconds(std::chrono::milliseconds(value));
    } else if (match[2] == "s") {
      time = Nanoseconds(std::chrono::seconds(value));
    } else {
      throw boost::program_options::invalid_option_value(token);
    }
  } else {
    throw boost::program_options::invalid_option_value(token);
  }
  return in;
}

std::ostream& operator<<(std::ostream& out, const Nanoseconds& time) {
  out << time.to_str();
  return out;
}

std::string Nanoseconds::to_str() const {
  std::stringstream out;
  // Convert the time to a human-readable format
  if (count() % UINT64_C(1000000000) == 0) {
    out << count() / UINT64_C(1000000000) << "s";
  } else if (count() % UINT64_C(1000000) == 0) {
    out << count() / UINT64_C(1000000) << "ms";
  } else if (count() % UINT64_C(1000) == 0) {
    out << count() / UINT64_C(1000) << "us";
  } else {
    out << count() << "ns";
  }
  return out.str();
}
