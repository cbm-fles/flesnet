/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "pgen_channel.hpp"
#include "MicrosliceDescriptor.hpp"
#include "System.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>

using namespace std::chrono_literals;

namespace cri {

pgen_channel::pgen_channel(std::span<fles::MicrosliceDescriptor> desc_buffer,
                           std::span<uint8_t> data_buffer,
                           uint64_t channel_index,
                           uint64_t duration_ns,
                           uint32_t typical_content_size,
                           uint32_t flags)
    : sw_dma_channel(
          desc_buffer, data_buffer, "pgen-" + std::to_string(channel_index)),
      m_channel_index(channel_index), m_duration_ns(duration_ns),
      m_typical_content_size(typical_content_size), m_flags(flags),
      m_random_distribution(typical_content_size) {
  start();
}

void pgen_channel::thread_work(std::stop_token stop_token) {
  // Microslices are stamped with the data clock, so that they match the
  // timeslices of the chain also when the data clock is not the system clock
  const auto& clock = fles::system::data_clock();
  uint64_t ms_time =
      fles::system::current_time_ns() / m_duration_ns * m_duration_ns;

  while (!stop_token.stop_requested()) {
    uint64_t current_time = fles::system::current_time_ns();
    if (current_time >= ms_time) {
      while (current_time >= ms_time) {
        generate_microslice(ms_time);
        ms_time += m_duration_ns;
      }
    } else {
      // bounded, so that a stop request is seen in time also when the data
      // clock runs slowly or has not started yet
      std::this_thread::sleep_for(
          std::min(clock.real_duration(ms_time - current_time),
                   std::chrono::nanoseconds(100ms)));
    }
  }
}

void pgen_channel::generate_microslice(uint64_t time_ns) {
  unsigned int content_bytes = m_typical_content_size;
  if (has_flag(PgenFlags::RandomizeSizes)) {
    content_bytes = m_random_distribution(m_random_generator);
  }
  content_bytes &= ~0x7u; // Round down to multiple of sizeof(uint64_t)

  if (!fits(content_bytes)) {
    skip(content_bytes);
    return;
  }

  fles::MicrosliceDescriptor desc{};
  desc.hdr_id = static_cast<uint8_t>(fles::HeaderFormatIdentifier::Standard);
  desc.hdr_ver = static_cast<uint8_t>(fles::HeaderFormatVersion::Standard);
  desc.eq_id = 0xE001;
  desc.flags = 0x0000;
  desc.sys_id = static_cast<uint8_t>(fles::Subsystem::FLES);
  desc.sys_ver =
      static_cast<uint8_t>(has_flag(PgenFlags::GeneratePattern)
                               ? fles::SubsystemFormatFLES::BasicRampPattern
                               : fles::SubsystemFormatFLES::Uninitialized);
  desc.idx = time_ns;
  desc.crc = 0x00000000;
  desc.size = content_bytes;

  if (!has_flag(PgenFlags::GeneratePattern)) {
    append(desc,
           [](uint8_t* /*dst*/, std::size_t /*offset*/, std::size_t /*n*/) {});
    return;
  }

  // The content is a ramp of 64-bit words, each the channel index and the
  // offset of the word in the content; the crc field holds a checksum of the
  // words (see FlesnetPatternChecker)
  append(desc, [this, &desc](uint8_t* dst, std::size_t offset, std::size_t n) {
    const std::size_t end = offset + n;
    std::size_t pos = offset;
    while (pos < end) {
      const uint64_t word_offset = pos & ~uint64_t{7};
      const uint64_t word = (m_channel_index << 48U) | word_offset;
      const std::size_t in_word = pos - word_offset;
      const std::size_t len = std::min(sizeof(word) - in_word, end - pos);
      std::memcpy(dst + (pos - offset),
                  reinterpret_cast<const uint8_t*>(&word) + in_word, len);
      if (in_word + len == sizeof(word)) {
        desc.crc ^= static_cast<uint32_t>(word & 0xffffffffU) ^
                    static_cast<uint32_t>(word >> 32U);
      }
      pos += len;
    }
  });
}

} // namespace cri
