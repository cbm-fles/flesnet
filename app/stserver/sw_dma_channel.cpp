/* Copyright (C) 2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "sw_dma_channel.hpp"
#include "log.hpp"
#include <utility>

namespace cri {

sw_dma_channel::sw_dma_channel(
    std::span<fles::MicrosliceDescriptor> desc_buffer,
    std::span<uint8_t> data_buffer,
    std::string name)
    : m_desc_buffer(desc_buffer.data(), desc_buffer.size()),
      m_data_buffer(data_buffer.data(), data_buffer.size()),
      m_name(std::move(name)) {}

void sw_dma_channel::start() {
  m_worker_thread = std::jthread([this](std::stop_token stop_token) {
    logging::set_thread_name(m_name);
    thread_work(stop_token);
  });
}

void sw_dma_channel::stop() {
  if (m_worker_thread.joinable()) {
    m_worker_thread.request_stop();
    m_worker_thread.join();
  }
}

void sw_dma_channel::set_sw_read_pointers(uint64_t data_offset,
                                          uint64_t desc_offset) {
  // The hardware interface hands over byte offsets into the buffers. They
  // advance by less than one buffer size per call, which allows converting
  // them back into absolute indices.
  auto update_index = [](uint64_t new_offset, uint64_t& current_offset,
                         size_t buffer_size, std::atomic<uint64_t>& read_index,
                         size_t element_size) {
    uint64_t diff = 0;
    if (new_offset > current_offset) {
      diff = new_offset - current_offset;
    } else if (new_offset < current_offset) {
      diff = new_offset + buffer_size - current_offset;
    }
    read_index.fetch_add(diff / element_size, std::memory_order_relaxed);
    current_offset = new_offset;
  };

  update_index(desc_offset, m_desc_offset, m_desc_buffer.bytes(),
               m_desc_read_index, sizeof(fles::MicrosliceDescriptor));
  update_index(data_offset, m_data_offset, m_data_buffer.bytes(),
               m_data_read_index, sizeof(uint8_t));
}

uint64_t sw_dma_channel::get_desc_index() {
  return m_desc_write_index.load(std::memory_order_acquire);
}

bool sw_dma_channel::fits(std::size_t content_bytes) const {
  const uint64_t data_used =
      m_data_write_index - m_data_read_index.load(std::memory_order_relaxed);
  const uint64_t desc_used =
      m_desc_write_index.load(std::memory_order_relaxed) -
      m_desc_read_index.load(std::memory_order_relaxed);
  return data_used + content_bytes <= m_data_buffer.bytes() &&
         desc_used + 1 <= m_desc_buffer.size();
}

void sw_dma_channel::skip(std::size_t content_bytes) {
  m_skipped = true;
  m_skipped_microslice_count.fetch_add(1, std::memory_order_relaxed);
  m_skipped_byte_count.fetch_add(content_bytes, std::memory_order_relaxed);
}

} // namespace cri
