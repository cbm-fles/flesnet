/* Copyright (C) 2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Jan de Cuveland */
#pragma once

#include <cstddef>
#include <span>
#include <vector>

/// Maps ring buffers of a shared memory file a second time, each one twice in
/// a row. Any range of up to a ring's size is then contiguous in virtual
/// memory, also across the wrap-around, and can be sent as a single block.
///
/// All mirrors lie in one virtual memory region, which can be registered as a
/// whole. Each ring keeps the 2 MiB alignment of its file offset, so huge
/// pages of the file can be mapped as such.
class RingMirror {
public:
  struct Ring {
    std::size_t offset; ///< offset of the ring in the file (page aligned)
    std::size_t size;   ///< size of the ring in bytes (multiple of page size)
  };

  /// Map the rings of the file `fd`. Throws std::system_error or
  /// std::invalid_argument if this is not possible.
  RingMirror(int fd, std::span<const Ring> rings);
  ~RingMirror();

  RingMirror(const RingMirror&) = delete;
  RingMirror& operator=(const RingMirror&) = delete;

  /// Start of the mirror of ring `i`, valid for twice the ring's size.
  [[nodiscard]] std::byte* ring(std::size_t i) const { return m_rings.at(i); }

  /// The virtual memory region holding all mirrors.
  [[nodiscard]] std::span<std::byte> region() const {
    return {m_region, m_region_size};
  }

private:
  std::byte* m_mapping = nullptr; ///< start of the whole mapping
  std::size_t m_mapping_size = 0;
  std::byte* m_region = nullptr; ///< start of the first mirror
  std::size_t m_region_size = 0;
  std::vector<std::byte*> m_rings;
};
