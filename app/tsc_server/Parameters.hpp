// Copyright 2025 Dirk Hutter, Jan de Cuveland
#pragma once

#include "OptionValues.hpp"
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

struct pci_addr {
public:
  pci_addr(uint8_t bus = 0, uint8_t dev = 0, uint8_t func = 0)
      : bus(bus), dev(dev), func(func) {}
  uint8_t bus;
  uint8_t dev;
  uint8_t func;
};

class Parameters {

public:
  Parameters(int argc, char* argv[]) { parse_options(argc, argv); }

  Parameters(const Parameters&) = delete;
  void operator=(const Parameters&) = delete;

  [[nodiscard]] std::string monitor_uri() const { return m_monitor_uri; }

  [[nodiscard]] bool device_autodetect() const { return m_device_autodetect; }
  [[nodiscard]] pci_addr device_address() const { return m_device_address; }
  [[nodiscard]] std::string shm_id() const { return m_shm_id; }
  [[nodiscard]] uint16_t listen_port() const { return m_listen_port; }
  [[nodiscard]] std::string tssched_address() const {
    return m_tssched_address;
  }

  // Pattern generator parameters
  [[nodiscard]] uint32_t pgen_channels() const { return m_pgen_channels; }
  [[nodiscard]] int64_t pgen_microslice_duration_ns() const {
    return m_pgen_microslice_duration.count();
  }
  [[nodiscard]] size_t pgen_microslice_size() const {
    return m_pgen_microslice_size;
  }
  [[nodiscard]] uint32_t pgen_flags() const { return m_pgen_flags; }

  // Global parameters
  [[nodiscard]] int64_t timeslice_duration_ns() const {
    return m_timeslice_duration.count();
  }
  [[nodiscard]] int64_t timeout_ns() const { return m_timeout.count(); }

  // Channel parameters (may be set individually in the future)
  [[nodiscard]] size_t data_buffer_size() const { return m_data_buffer_size; }
  [[nodiscard]] size_t desc_buffer_size() const { return m_desc_buffer_size; }
  [[nodiscard]] int64_t overlap_before_ns() const {
    return m_overlap_before.count();
  }
  [[nodiscard]] int64_t overlap_after_ns() const {
    return m_overlap_after.count();
  }

private:
  void parse_options(int argc, char* argv[]);
  [[nodiscard]] std::string buffer_info() const;

  std::string m_monitor_uri;

  bool m_device_autodetect = true;
  pci_addr m_device_address;
  std::string m_shm_id;
  uint16_t m_listen_port = DEFAULT_SENDER_PORT;
  std::string m_tssched_address = "localhost";

  // Pattern generator parameters
  uint32_t m_pgen_channels = 0;
  Nanoseconds m_pgen_microslice_duration{125us};
  size_t m_pgen_microslice_size = 100000; // 100 kB
  uint32_t m_pgen_flags = 0;

  // Global parameters
  Nanoseconds m_timeslice_duration{100ms};
  Nanoseconds m_timeout{1ms};

  // Channel parameters (may be set individually in the future)
  size_t m_data_buffer_size = UINT64_C(1) << 28; // 256 MiB
  size_t m_desc_buffer_size = UINT64_C(1) << 19; // 512 ki entries
  Nanoseconds m_overlap_before{100us};
  Nanoseconds m_overlap_after{100us};
};
