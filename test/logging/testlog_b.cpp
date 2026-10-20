// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
// Test library that also uses the format-style calls, is built with hidden
// visibility, and logs while being loaded.

#include "log.hpp"

namespace {
// Only throws if allocating memory fails while the library is loaded
struct LogOnLoad {
  LogOnLoad() { L_(info) << "b loaded"; }
} const log_on_load; // NOLINT(cert-err58-cpp)
} // namespace

extern "C" {

__attribute__((visibility("default"))) void testlog_b_log(int i) {
  L_(info) << "from b " << i;
  LF(info, "formatted from b {}", i);
}
}
