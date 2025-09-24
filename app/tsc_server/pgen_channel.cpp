/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "pgen_channel.hpp"
#include "MicrosliceDescriptor.hpp"
#include "monitoring/System.hpp"
#include <chrono>
#include <sys/types.h>

namespace {
[[maybe_unused]] inline uint64_t
chrono_to_timestamp(std::chrono::time_point<std::chrono::high_resolution_clock,
                                            std::chrono::nanoseconds> time) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             time.time_since_epoch())
      .count();
}
} // namespace

namespace cri {

pgen_channel::pgen_channel(std::span<fles::MicrosliceDescriptor> desc_buffer,
                           std::span<uint8_t> data_buffer,
                           uint64_t channel_index,
                           uint64_t duration_ns,
                           uint32_t typical_content_size,
                           uint32_t flags)
    : m_desc_buffer(desc_buffer.data(), desc_buffer.size()),
      m_data_buffer(data_buffer.data(), data_buffer.size()),
      m_channel_index(channel_index), m_duration_ns(duration_ns),
      m_typical_content_size(typical_content_size), m_flags(flags),
      m_random_distribution(typical_content_size),
      m_worker_thread(&pgen_channel::thread_work, this) {}

void pgen_channel::set_sw_read_pointers(uint64_t data_offset,
                                        uint64_t desc_offset) {
  // Helper lambda to update read indexes and cached offsets
  auto update_index = [](uint64_t new_offset, uint64_t& current_offset,
                         size_t buffer_size, std::atomic<uint64_t>& read_index,
                         size_t element_size) {
    int64_t diff = 0;
    if (new_offset > current_offset) {
      diff = new_offset - current_offset;
    } else if (new_offset < current_offset) {
      diff = new_offset + buffer_size - current_offset;
    }
    read_index += diff / element_size;
    current_offset = new_offset;
  };

  // Update descriptor pointers
  update_index(desc_offset, m_desc_offset, m_desc_buffer.bytes(),
               m_desc_read_index, sizeof(*m_desc_buffer.ptr()));

  // Update data pointers
  update_index(data_offset, m_data_offset, m_data_buffer.bytes(),
               m_data_read_index, sizeof(*m_data_buffer.ptr()));
}

uint64_t pgen_channel::get_desc_index() { return m_desc_write_index; }

void pgen_channel::thread_work(std::stop_token stop_token) {
  std::string thread_name = "pgen-" + std::to_string(m_channel_index);
  ::cbm::system::set_thread_name(thread_name);

  uint64_t ms_time =
      chrono_to_timestamp(std::chrono::high_resolution_clock::now()) /
      m_duration_ns * m_duration_ns;

  while (!stop_token.stop_requested()) {
    uint64_t current_time =
        chrono_to_timestamp(std::chrono::high_resolution_clock::now());
    if (current_time >= ms_time) {
      while (current_time >= ms_time) {
        generate_microslice(ms_time);
        ms_time += m_duration_ns;
      }
    } else {
      std::this_thread::sleep_for(
          std::chrono::nanoseconds(ms_time - current_time));
    }
  }
}

void pgen_channel::generate_microslice(uint64_t time_ns) {
  unsigned int content_bytes = m_typical_content_size;
  if (has_flag(PgenFlags::RandomizeSizes)) {
    content_bytes = m_random_distribution(m_random_generator);
  }
  content_bytes &= ~0x7u; // Round down to multiple of sizeof(uint64_t)

  // Check for space in data and descriptor buffers
  if ((m_data_write_index - m_data_read_index + content_bytes >
       m_data_buffer.bytes()) ||
      (m_desc_write_index - m_desc_read_index + 1 > m_desc_buffer.size())) {
    m_skipped_microslice = true;
    return;
  }

  const auto hdr_id =
      static_cast<uint8_t>(fles::HeaderFormatIdentifier::Standard);
  const auto hdr_ver =
      static_cast<uint8_t>(fles::HeaderFormatVersion::Standard);
  const uint16_t eq_id = 0xE001;
  uint16_t flags = 0x0000;
  if (m_skipped_microslice) {
    flags |= static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim);
    // TODO: introduce and use correct flag for skipped microslices
    // flags |= static_cast<uint16_t>(fles::MicrosliceFlags::SkippedMicroslice);
    m_skipped_microslice = false;
  }
  const auto sys_id = static_cast<uint8_t>(fles::Subsystem::FLES);
  const auto sys_ver =
      static_cast<uint8_t>(has_flag(PgenFlags::GeneratePattern)
                               ? fles::SubsystemFormatFLES::BasicRampPattern
                               : fles::SubsystemFormatFLES::Uninitialized);
  uint64_t idx = time_ns;
  uint32_t crc = 0x00000000;
  uint32_t size = content_bytes;
  uint64_t offset = m_data_write_index;

  // Write to data buffer
  if (has_flag(PgenFlags::GeneratePattern)) {
    bool is_contiguous =
        (m_data_buffer.offset_bytes(m_data_write_index) + content_bytes <=
         m_data_buffer.bytes());
    if (is_contiguous) {
      uint8_t* begin = &m_data_buffer.at(m_data_write_index);
      for (uint64_t i = 0; i < content_bytes; i += sizeof(uint64_t)) {
        uint64_t data_word = (m_channel_index << 48L) | i;
        *reinterpret_cast<uint64_t*>(begin + i) = data_word;
        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32L);
      }
      m_data_write_index += content_bytes;
    } else {
      for (uint64_t i = 0; i < content_bytes; i += sizeof(uint64_t)) {
        uint64_t data_word = (m_channel_index << 48L) | i;
        reinterpret_cast<uint64_t&>(m_data_buffer.at(m_data_write_index)) =
            data_word;
        m_data_write_index += sizeof(uint64_t);
        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32L);
      }
    }
  } else {
    m_data_write_index += content_bytes;
  }

  // Write to descriptor buffer
  const_cast<fles::MicrosliceDescriptor&>(
      m_desc_buffer.at(m_desc_write_index++)) =
      fles::MicrosliceDescriptor({hdr_id, hdr_ver, eq_id, flags, sys_id,
                                  sys_ver, idx, crc, size, offset});
}

} // namespace cri
