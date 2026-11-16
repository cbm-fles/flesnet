/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#pragma once

#include "sw_dma_channel.hpp"
#include <random>
#include <span>
#include <stop_token>

namespace cri {

// Flags for pattern generator channels
enum class PgenFlags : uint32_t {
  None = 0,

  // Generate a specific pattern in the data content
  GeneratePattern = 1 << 0,

  // Randomize the sizes of the microslices
  RandomizeSizes = 1 << 1
};

/// A software pattern generator channel: one microslice per duration, stamped
/// with the data clock.
class pgen_channel : public sw_dma_channel {
public:
  pgen_channel(std::span<fles::MicrosliceDescriptor> desc_buffer,
               std::span<uint8_t> data_buffer,
               uint64_t channel_index,
               uint64_t duration_ns,
               uint32_t typical_content_size,
               uint32_t flags);
  ~pgen_channel() override { stop(); }

private:
  uint64_t m_channel_index;
  uint64_t m_duration_ns;
  std::size_t m_typical_content_size;
  uint32_t m_flags;

  [[nodiscard]] bool has_flag(PgenFlags f) const {
    return (m_flags & static_cast<uint32_t>(f)) != 0;
  }

  std::default_random_engine m_random_generator;
  std::poisson_distribution<unsigned int> m_random_distribution;

  void thread_work(std::stop_token stop_token) override;
  void generate_microslice(uint64_t time_ns);
};

} // namespace cri
