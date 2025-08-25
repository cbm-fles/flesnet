// Copyright 2025 Jan de Cuveland
#pragma once

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
  [[nodiscard]] std::string tssched_address() const { return _tssched_address; }
  [[nodiscard]] int64_t timeout_ns() const { return _timeout.count(); }
  [[nodiscard]] std::string shm_id() const { return _shm_id; }
  [[nodiscard]] size_t buffer_size() const { return _buffer_size; }

private:
  void parse_options(int argc, char* argv[]);

  std::string _monitor_uri;
  std::string _tssched_address = "localhost";
  Nanoseconds _timeout{10s};
  std::string _shm_id = "flesnet_ts_builder";
  size_t _buffer_size = UINT64_C(1) << 35; // 32 GiB
};
