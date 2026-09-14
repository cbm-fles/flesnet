/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Jan de Cuveland, Dirk Hutter */
#pragma once

#include "SubTimeslice.hpp"
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <ucp/api/ucp.h>
#include <vector>

// Sender only: internal structures for transferring subtimeslice memory handles
// to the StSender

struct StComponentHandle {
  std::vector<ucp_dt_iov> ms_data;
  std::size_t num_microslices = 0;
  uint32_t flags = 0;

  void set_flag(TsComponentFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(TsComponentFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(TsComponentFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  /// The number of microslice data (descriptors + contents) bytes
  [[nodiscard]] uint64_t ms_data_size() const {
    uint64_t size = 0;
    for (const auto& sg : ms_data) {
      size += sg.length;
    }
    return size;
  }

  /// Dump contents (for debugging).
  friend std::ostream& operator<<(std::ostream& os,
                                  const StComponentHandle& i) {
    return os << "StComponentHandle(num_microslices=" << i.num_microslices
              << ", flags=" << i.flags << ")";
  }
};

struct StHandle {
  uint64_t start_time_ns = 0;
  uint64_t duration_ns = 0;
  uint32_t flags = 0;
  std::vector<StComponentHandle> components;

  void set_flag(TsFlag f) { flags |= static_cast<uint32_t>(f); }
  void clear_flag(TsFlag f) { flags &= ~static_cast<uint32_t>(f); }
  [[nodiscard]] bool has_flag(TsFlag f) const {
    return (flags & static_cast<uint32_t>(f)) != 0;
  }

  /// Dump contents (for debugging)
  friend std::ostream& operator<<(std::ostream& os, const StHandle& i) {
    return os << "StHandle(start_time_ns=" << i.start_time_ns
              << ", duration_ns=" << i.duration_ns << ", flags=" << i.flags
              << ", components=...)";
  }
};
