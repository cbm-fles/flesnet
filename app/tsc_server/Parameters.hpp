// Copyright 2025 Dirk Hutter, Jan de Cuveland
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

struct pci_addr {
public:
  pci_addr(uint8_t bus = 0, uint8_t dev = 0, uint8_t func = 0)
      : bus(bus), dev(dev), func(func) {}
  uint8_t bus;
  uint8_t dev;
  uint8_t func;
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

  [[nodiscard]] bool device_autodetect() const { return _device_autodetect; }
  [[nodiscard]] pci_addr device_address() const { return _device_address; }
  [[nodiscard]] std::string shm_id() const { return _shm_id; }
  [[nodiscard]] uint16_t listen_port() const { return _listen_port; }
  [[nodiscard]] std::string tssched_address() const { return _tssched_address; }

  // Pattern generator parameters
  [[nodiscard]] uint32_t pgen_channels() const { return _pgen_channels; }
  [[nodiscard]] int64_t pgen_microslice_duration_ns() const {
    return _pgen_microslice_duration.count();
  }
  [[nodiscard]] size_t pgen_microslice_size() const {
    return _pgen_microslice_size;
  }
  [[nodiscard]] uint32_t pgen_flags() const { return _pgen_flags; }

  // Global parameters
  [[nodiscard]] int64_t timeslice_duration_ns() const {
    return _timeslice_duration.count();
  }
  [[nodiscard]] int64_t timeout_ns() const { return _timeout.count(); }

  // Channel parameters (may be set individually in the future)
  [[nodiscard]] size_t data_buffer_size() const { return _data_buffer_size; }
  [[nodiscard]] size_t desc_buffer_size() const { return _desc_buffer_size; }
  [[nodiscard]] int64_t overlap_before_ns() const {
    return _overlap_before.count();
  }
  [[nodiscard]] int64_t overlap_after_ns() const {
    return _overlap_after.count();
  }

private:
  void parse_options(int argc, char* argv[]);
  [[nodiscard]] std::string buffer_info() const;

  std::string _monitor_uri;

  bool _device_autodetect = true;
  pci_addr _device_address;
  std::string _shm_id;
  uint16_t _listen_port = 0;
  std::string _tssched_address;

  // Pattern generator parameters
  uint32_t _pgen_channels = 0;
  Nanoseconds _pgen_microslice_duration{125us};
  size_t _pgen_microslice_size = 100000; // 100 kB
  uint32_t _pgen_flags = 0;

  // Global parameters
  Nanoseconds _timeslice_duration{100ms};
  Nanoseconds _timeout{1ms};

  // Channel parameters (may be set individually in the future)
  size_t _data_buffer_size = UINT64_C(1) << 27; // 128 MiB
  size_t _desc_buffer_size = UINT64_C(1) << 19; // 512 ki entries
  Nanoseconds _overlap_before{100us};
  Nanoseconds _overlap_after{100us};
};
