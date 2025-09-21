// Copyright 2025 Jan de Cuveland

#include "OptionValues.hpp"
#include <cmath>
#include <regex>
#include <sstream>
#include <stdexcept>

// Convert the time to a human-readable format
std::string Nanoseconds::to_string() const {
  std::stringstream out;
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

// Parse the time with a suffix
Nanoseconds Nanoseconds::parse(const std::string& str) {
  std::regex pattern("(-?[0-9]+)(ns|us|µs|ms|s)");
  std::smatch match;

  if (!std::regex_match(str, match, pattern)) {
    throw std::invalid_argument("Invalid time format: " + str);
  }

  long long number = std::stoll(match[1]);
  std::string suffix = match[2].str();

  if (suffix == "ns") {
    return {std::chrono::nanoseconds(number)};
  }
  if (suffix == "us" || suffix == "µs") {
    return {std::chrono::microseconds(number)};
  }
  if (suffix == "ms") {
    return {std::chrono::milliseconds(number)};
  }
  if (suffix == "s") {
    return {std::chrono::seconds(number)};
  }
  throw std::invalid_argument("Unknown time suffix: " + suffix);
}

std::string SizeValue::to_string() const {
  if (m_value == 0) {
    return "0";
  }

  const std::pair<size_t, const char*> binary_units[] = {
      {1024ULL * 1024 * 1024 * 1024 * 1024, "PiB"},
      {1024ULL * 1024 * 1024 * 1024, "TiB"},
      {1024ULL * 1024 * 1024, "GiB"},
      {1024ULL * 1024, "MiB"},
      {1024ULL, "KiB"}};

  for (const auto& unit : binary_units) {
    if (m_value >= unit.first && m_value % unit.first == 0) {
      return std::to_string(m_value / unit.first) + unit.second;
    }
  }

  const std::pair<size_t, const char*> decimal_units[] = {
      {1000ULL * 1000 * 1000 * 1000 * 1000, "PB"},
      {1000ULL * 1000 * 1000 * 1000, "TB"},
      {1000ULL * 1000 * 1000, "GB"},
      {1000ULL * 1000, "MB"},
      {1000ULL, "kB"}};

  for (const auto& unit : decimal_units) {
    if (m_value >= unit.first && m_value % unit.first == 0) {
      return std::to_string(m_value / unit.first) + unit.second;
    }
  }

  // If no clean unit conversion, show as bytes
  return std::to_string(m_value) + "B";
}

SizeValue SizeValue::parse(const std::string& str) {
  std::regex pattern(
      R"(^\s*([0-9]*\.?[0-9]+)\s*([kKmMgGtTpP]?[iI]?[bB]?)\s*$)");
  std::smatch match;

  if (!std::regex_match(str, match, pattern)) {
    throw std::invalid_argument("Invalid size format: " + str);
  }

  double number = std::stod(match[1].str());
  std::string suffix = match[2].str();

  std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);

  size_t multiplier = 1;

  if (suffix.empty() || suffix == "b") {
    multiplier = 1;
  } else if (suffix == "kb" || suffix == "k") {
    multiplier = 1000ULL;
  } else if (suffix == "kib" || suffix == "ki") {
    multiplier = 1024ULL;
  } else if (suffix == "mb" || suffix == "m") {
    multiplier = 1000ULL * 1000;
  } else if (suffix == "mib" || suffix == "mi") {
    multiplier = 1024ULL * 1024;
  } else if (suffix == "gb" || suffix == "g") {
    multiplier = 1000ULL * 1000 * 1000;
  } else if (suffix == "gib" || suffix == "gi") {
    multiplier = 1024ULL * 1024 * 1024;
  } else if (suffix == "tb" || suffix == "t") {
    multiplier = 1000ULL * 1000 * 1000 * 1000;
  } else if (suffix == "tib" || suffix == "ti") {
    multiplier = 1024ULL * 1024 * 1024 * 1024;
  } else if (suffix == "pb" || suffix == "p") {
    multiplier = 1000ULL * 1000 * 1000 * 1000 * 1000;
  } else if (suffix == "pib" || suffix == "pi") {
    multiplier = 1024ULL * 1024 * 1024 * 1024 * 1024;
  } else {
    throw std::invalid_argument("Unknown size suffix: " + suffix);
  }

  if (number > static_cast<double>(SIZE_MAX) / multiplier) {
    throw std::overflow_error("Size value too large");
  }

  return SizeValue(static_cast<size_t>(number * multiplier));
}
