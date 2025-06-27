// Copyright 2025 Jan de Cuveland

#include "dma_channel_pgen.hpp"
#include <chrono>

namespace cri {

dma_channel_pgen::dma_channel_pgen(
    std::span<fles::MicrosliceDescriptor> desc_buffer,
    std::span<uint8_t> data_buffer,
    uint64_t microslice_time_ns,
    std::size_t microslice_size)
    : m_worker_thread(&dma_channel_pgen::thread_work, this, std::stop_token{}) {
  // ...
}

void dma_channel_pgen::set_sw_read_pointers(uint64_t data_offset,
                                            uint64_t desc_offset) {
  // ...
}

void dma_channel_pgen::thread_work(std::stop_token stop_token) {
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
