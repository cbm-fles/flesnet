// Copyright 2025 Jan de Cuveland
#pragma once

#include "TsbProtocol.hpp"
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;

/// Run parameters exception class.
class ParametersException : public std::runtime_error {
public:
  explicit ParametersException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};

class Nanoseconds {
private:
  int64_t value;

public:
  Nanoseconds() : value(0) {}
  Nanoseconds(std::chrono::nanoseconds val) : value(val.count()) {}
  Nanoseconds(int64_t val) : value(val) {}

  [[nodiscard]] int64_t count() const { return value; }
  [[nodiscard]] std::string to_str() const;

  operator std::chrono::nanoseconds() const {
    return std::chrono::nanoseconds(value);
  }
  friend std::istream& operator>>(std::istream& in, Nanoseconds& time);
  friend std::ostream& operator<<(std::ostream& out, const Nanoseconds& time);
};

class Parameters {

public:
  Parameters(int argc, char* argv[]) { parse_options(argc, argv); }

  Parameters(const Parameters&) = delete;
  void operator=(const Parameters&) = delete;

  [[nodiscard]] std::string monitor_uri() const { return _monitor_uri; }
  [[nodiscard]] uint16_t listen_port() const { return _listen_port; }
  [[nodiscard]] int64_t timeslice_duration_ns() const {
    return _timeslice_duration.count();
  }
  [[nodiscard]] int64_t timeout_ns() const { return _timeout.count(); }

private:
  void parse_options(int argc, char* argv[]);

  std::string _monitor_uri;
  uint16_t _listen_port = DEFAULT_SCHEDULER_PORT;
  Nanoseconds _timeslice_duration{100ms};
  Nanoseconds _timeout{100ms};
};
