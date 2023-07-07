// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

/// Run parameter exception class.
class ParametersException : public std::runtime_error {
public:
  explicit ParametersException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};

/// Global run parameter class.
class Parameters {
public:
  Parameters(int argc, char* argv[]) { parse_options(argc, argv); }

  Parameters(const Parameters&) = delete;
  void operator=(const Parameters&) = delete;

  [[nodiscard]] std::string monitor_uri() const { return monitor_uri_; }

  [[nodiscard]] int32_t client_index() const { return client_index_; }

  [[nodiscard]] std::string input_uri() const { return input_uri_; }

  [[nodiscard]] std::string output_archive() const { return output_archive_; }

  [[nodiscard]] size_t output_archive_items() const {
    return output_archive_items_;
  }

  [[nodiscard]] size_t output_archive_bytes() const {
    return output_archive_bytes_;
  }

  [[nodiscard]] bool analyze() const { return analyze_; }

  [[nodiscard]] bool benchmark() const { return benchmark_; }

  [[nodiscard]] size_t verbosity() const { return verbosity_; }

  [[nodiscard]] bool histograms() const { return histograms_; }

  [[nodiscard]] std::string publish_address() const { return publish_address_; }

  [[nodiscard]] uint32_t publish_hwm() const { return publish_hwm_; }

  [[nodiscard]] uint64_t maximum_number() const { return maximum_number_; }

  [[nodiscard]] uint64_t offset() const { return offset_; }

  [[nodiscard]] uint64_t stride() const { return stride_; }

  [[nodiscard]] double rate_limit() const { return rate_limit_; }

  [[nodiscard]] double native_speed() const { return native_speed_; }

private:
  void parse_options(int argc, char* argv[]);

  std::string monitor_uri_;

  int32_t client_index_ = -1;
  std::string input_uri_;
  std::string output_archive_;
  size_t output_archive_items_ = SIZE_MAX;
  size_t output_archive_bytes_ = SIZE_MAX;
  bool analyze_ = false;
  bool benchmark_ = false;
  size_t verbosity_ = 0;
  bool histograms_ = false;
  std::string publish_address_;
  uint32_t publish_hwm_ = 1;
  uint64_t maximum_number_ = UINT64_MAX;
  uint64_t offset_ = 0;
  uint64_t stride_ = 1;
  double rate_limit_ = 0.0;
  double native_speed_ = 0.0;
};
