// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
// Helper for test_logging_core: a fatal message must be written even if the
// process aborts right after logging it.

#include "log.hpp"

#include <cstdlib>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    return EXIT_FAILURE;
  }
  logging::Options options;
  options.level = fatal; // keep stderr quiet
  options.file_level = info;
  options.file = argv[1];
  logging::setup(options);
  for (int i = 0; i < 1000; ++i) {
    L_(info) << "filler " << i;
  }
  L_(fatal) << "about to abort";
  std::abort();
}
