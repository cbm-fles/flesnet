// Copyright 2025 Jan de Cuveland
#pragma once

#include "OptionValues.hpp"
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

class Parameters {

public:
  Parameters(int argc, char* argv[]) { parse_options(argc, argv); }

  Parameters(const Parameters&) = delete;
  void operator=(const Parameters&) = delete;

  [[nodiscard]] std::string monitor_uri() const { return m_monitor_uri; }
  [[nodiscard]] std::string tssched_address() const {
    return m_tssched_address;
  }
  [[nodiscard]] int64_t timeout_ns() const { return m_timeout.count(); }
  [[nodiscard]] std::string shm_id() const { return m_shm_id; }
  [[nodiscard]] size_t buffer_size() const { return m_buffer_size; }

private:
  void parse_options(int argc, char* argv[]);

  std::string m_monitor_uri;
  std::string m_tssched_address = "localhost";
  Nanoseconds m_timeout{10s};
  std::string m_shm_id = "flesnet_ts_builder";
  size_t m_buffer_size = UINT64_C(1) << 35; // 32 GiB
};
