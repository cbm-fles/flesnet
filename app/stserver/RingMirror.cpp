/* Copyright (C) 2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Jan de Cuveland */

#include "RingMirror.hpp"
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <sys/mman.h>
#include <system_error>
#include <unistd.h>

namespace {
constexpr std::size_t huge_page_size = std::size_t{2} << 20;
} // namespace

RingMirror::RingMirror(int fd, std::span<const Ring> rings) {
  const auto page_size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));

  // Lay out the mirrors: each one at the next address that has the same
  // position within a huge page as the ring's offset in the file
  std::vector<std::size_t> starts;
  std::size_t end = 0;
  for (const auto& r : rings) {
    if (r.size == 0 || r.offset % page_size != 0 || r.size % page_size != 0) {
      throw std::invalid_argument(
          "ring offset and size must be multiples of the page size");
    }
    std::size_t start =
        end + (r.offset - end % huge_page_size) % huge_page_size;
    starts.push_back(start);
    end = start + 2 * r.size;
  }
  if (starts.empty()) {
    throw std::invalid_argument("no rings to map");
  }

  // Reserve the address range with plain memory, so that the gaps between
  // the mirrors can be registered along with them
  m_mapping_size = end + huge_page_size;
  void* p = mmap(nullptr, m_mapping_size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (p == MAP_FAILED) {
    throw std::system_error(errno, std::generic_category(), "mmap (reserve)");
  }
  m_mapping = static_cast<std::byte*>(p);
  auto base =
      (reinterpret_cast<std::uintptr_t>(m_mapping) + huge_page_size - 1) &
      ~(huge_page_size - 1);
  m_region = reinterpret_cast<std::byte*>(base) + starts.front();
  m_region_size = end - starts.front();

  for (std::size_t i = 0; i < rings.size(); ++i) {
    std::byte* ring = reinterpret_cast<std::byte*>(base) + starts[i];
    for (std::byte* copy : {ring, ring + rings[i].size}) {
      void* q =
          mmap(copy, rings[i].size, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_FIXED, fd, static_cast<off_t>(rings[i].offset));
      if (q == MAP_FAILED) {
        int err = errno;
        munmap(m_mapping, m_mapping_size);
        throw std::system_error(err, std::generic_category(), "mmap (ring)");
      }
    }
    m_rings.push_back(ring);
  }
}

RingMirror::~RingMirror() { munmap(m_mapping, m_mapping_size); }
