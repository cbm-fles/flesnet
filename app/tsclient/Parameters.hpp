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

  int32_t client_index() const { return client_index_; }

  std::string shm_identifier() const { return shm_identifier_; }

  std::string input_archive() const { return input_archive_; }

  uint64_t input_archive_cycles() const { return input_archive_cycles_; }

  std::string output_archive() const { return output_archive_; }

  size_t output_archive_items() const { return output_archive_items_; }

  size_t output_archive_bytes() const { return output_archive_bytes_; }

  bool analyze() const { return analyze_; }

  bool benchmark() const { return benchmark_; }

  size_t verbosity() const { return verbosity_; }

  std::string publish_address() const { return publish_address_; }

  uint32_t publish_hwmark() const { return publish_hwmark_; }

  std::string subscribe_address() const { return subscribe_address_; }

  uint32_t subscribe_hwmark() const { return subscribe_hwmark_; }

  uint64_t maximum_number() const { return maximum_number_; }

  double rate_limit() const { return rate_limit_; }

private:
  void parse_options(int argc, char* argv[]);

  int32_t client_index_ = -1;
  std::string shm_identifier_;
  std::string input_archive_;
  uint64_t input_archive_cycles_ = 1;
  std::string output_archive_;
  size_t output_archive_items_ = SIZE_MAX;
  size_t output_archive_bytes_ = SIZE_MAX;
  bool analyze_ = false;
  bool benchmark_ = false;
  size_t verbosity_ = 0;
  std::string publish_address_;
  uint32_t publish_hwmark_ = 1;
  std::string subscribe_address_;
  uint32_t subscribe_hwmark_ = 1;
  uint64_t maximum_number_ = UINT64_MAX;
  double rate_limit_ = 0.0;
};
