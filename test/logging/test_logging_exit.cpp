// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
// Helper for test_logging_core: a message logged during static destruction,
// after the writer thread has stopped, must reach the outputs like any other.

#include "log.hpp"

#include <cstdlib>

namespace {
// Constructed before logging starts, so destroyed after it stops
struct Late {
  ~Late() { L_(warning) << "after the writer stopped"; }
  Late() = default;
  Late(Late const&) = delete;
  Late& operator=(Late const&) = delete;
} const late;
} // namespace

int main(int argc, char* argv[]) {
  if (argc != 2) {
    return EXIT_FAILURE;
  }
  logging::Options options;
  options.level = fatal; // keep stderr quiet
  options.file_level = info;
  options.file = argv[1];
  logging::setup(options);
  L_(info) << "before exit";
  return EXIT_SUCCESS;
}
