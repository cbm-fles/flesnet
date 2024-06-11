// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
/// Run parameters exception class.
class ParametersException : public std::runtime_error {
public:
  explicit ParametersException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};

/// Global run parameters.
struct Parameters {
  Parameters(int argc, char* argv[]) { parse_options(argc, argv); }
  void parse_options(int argc, char* argv[]);

  // archive validation options
  uint64_t timeslice_size;
  uint64_t timeslice_cnt;
  uint64_t overlap = 1;
  uint64_t max_threads = 1;
  bool skip_metadata = false;
  
  std::vector<std::string> output_archives;
  std::vector<std::string> input_archives;
};
