// Copyright 2015 Dirk Hutter
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

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

  [[nodiscard]] std::string monitor_uri() const { return _monitor_uri; }
  [[nodiscard]] bool device_autodetect() const { return _device_autodetect; }
  [[nodiscard]] pci_addr device_address() const { return _device_address; }
  [[nodiscard]] std::string shm_id() const { return _shm_id; }
  [[nodiscard]] size_t data_buffer_size_exp() const {
    return _data_buffer_size_exp;
  }
  [[nodiscard]] size_t desc_buffer_size_exp() const {
    return _desc_buffer_size_exp;
  }

  [[nodiscard]] uint64_t overlap_before_ns() const {
    return (_overlap_before_ns);
  }
  [[nodiscard]] uint64_t overlap_after_ns() const {
    return (_overlap_after_ns);
  }
  [[nodiscard]] uint64_t timeslice_duration_ns() const {
    return (_timeslice_duration_ns);
  }
  [[nodiscard]] uint64_t timeslice_timeout_ns() const {
    return (_timeslice_timeout_ns);
  }

  [[nodiscard]] std::string print_buffer_info() const;

private:
  void parse_options(int argc, char* argv[]);

  std::string _monitor_uri;

  bool _device_autodetect = true;
  pci_addr _device_address;
  std::string _shm_id;
  size_t _data_buffer_size_exp = 0;
  size_t _desc_buffer_size_exp = 0;

  // TODO: parse as per-channel input parameters
  uint64_t _overlap_before_ns = 0;
  uint64_t _overlap_after_ns = 0;

  uint64_t _timeslice_duration_ns = 100e6; // 100 ms
  uint64_t _timeslice_timeout_ns = 1e9;    // 1 s
};
