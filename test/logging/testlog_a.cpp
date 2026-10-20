// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
// Test library that logs only through log.hpp, like most consumers.

#include "log.hpp"

extern "C" {

void testlog_a_setup(char const* filename) {
  logging::Options options;
  options.level = error; // keep stderr quiet
  options.file = filename;
  options.file_level = debug;
  logging::setup(options);
}

void testlog_a_log(int i) { L_(info) << "from a " << i; }

void testlog_a_flush() { logging::flush(); }
}
