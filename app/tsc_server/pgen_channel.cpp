// Copyright 2025 Jan de Cuveland

#include "pgen_channel.hpp"
#include <chrono>
#include <sys/types.h>

namespace cri {

pgen_channel::pgen_channel(std::span<fles::MicrosliceDescriptor> desc_buffer,
                           std::span<uint8_t> data_buffer,
                           uint64_t microslice_time_ns,
                           std::size_t microslice_size)
    : m_desc_buffer(desc_buffer.data(), desc_buffer.size()),
      m_data_buffer(data_buffer.data(), data_buffer.size()),
      m_microslice_time_ns(microslice_time_ns),
      m_microslice_size(microslice_size),
      m_worker_thread(&pgen_channel::thread_work, this, std::stop_token{}) {
  // ...
}

void pgen_channel::set_sw_read_pointers(uint64_t data_offset,
                                        uint64_t desc_offset) {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Helper lambda to update read indexes and cached offsets
  auto update_index = [](uint64_t new_offset, uint64_t& current_offset,
                         size_t buffer_size, uint64_t& read_index,
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

uint64_t pgen_channel::get_desc_index() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_desc_write_index;
}

void pgen_channel::thread_work(std::stop_token stop_token) {
  // This is where your thread's main work happens
  while (!stop_token.stop_requested()) {
    // Perform channel operations
    // ...

    // Sleep to avoid excessive CPU usage
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Optional: cleanup code when thread is stopping
}

} // namespace cri
