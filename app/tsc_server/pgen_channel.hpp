// Copyright 2025 Jan de Cuveland
#pragma once

#include "dma_channel.hpp"
#include "fles_core/RingBufferView.hpp"
#include "fles_ipc/MicrosliceDescriptor.hpp"
#include <atomic>
#include <random>
#include <span>
#include <thread>

namespace cri {

// Flags for pattern generator channels
enum class PgenFlags : uint32_t {
  None = 0,

  // Generate a specific pattern in the data content
  GeneratePattern = 1 << 0,

  // Randomize the sizes of the microslices
  RandomizeSizes = 1 << 1
};

class pgen_channel : public basic_dma_channel {
public:
  pgen_channel(std::span<fles::MicrosliceDescriptor> desc_buffer,
               std::span<uint8_t> data_buffer,
               uint64_t channel_index,
               uint64_t duration_ns,
               uint32_t typical_content_size,
               uint32_t flags);
  ~pgen_channel() override { m_worker_thread.request_stop(); };
  void set_sw_read_pointers(uint64_t data_offset,
                            uint64_t desc_offset) override;
  uint64_t get_desc_index() override;

private:
  RingBufferView<fles::MicrosliceDescriptor, false> m_desc_buffer;
  RingBufferView<uint8_t, false> m_data_buffer;
  uint64_t m_channel_index;
  uint64_t m_duration_ns;
  std::size_t m_typical_content_size;
  uint32_t m_flags;

  [[nodiscard]] bool has_flag(PgenFlags f) const {
    return (m_flags & static_cast<uint32_t>(f)) != 0;
  }

  std::atomic<uint64_t> m_desc_write_index = 0;
  uint64_t m_data_write_index = 0; // Only used by internal thread
  std::atomic<uint64_t> m_desc_read_index = 0;
  std::atomic<uint64_t> m_data_read_index = 0;
  uint64_t m_desc_offset = 0; // Only used by external thread
  uint64_t m_data_offset = 0; // Only used by external thread

  std::default_random_engine m_random_generator;
  std::poisson_distribution<unsigned int> m_random_distribution;

  std::jthread m_worker_thread;
  void thread_work(std::stop_token stop_token);
  void generate_microslice(uint64_t time_ns);

  bool m_skipped_microslice = false;
};

} // namespace cri
