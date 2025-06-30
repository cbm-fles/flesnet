// Copyright 2025 Jan de Cuveland
#pragma once

#include "dma_channel.hpp"
#include "fles_core/RingBufferView.hpp"
#include "fles_ipc/MicrosliceDescriptor.hpp"
#include <span>
#include <thread>

namespace cri {

class dma_channel_pgen : public basic_dma_channel {
public:
  dma_channel_pgen(std::span<fles::MicrosliceDescriptor> desc_buffer,
                   std::span<uint8_t> data_buffer,
                   uint64_t microslice_time_ns,
                   std::size_t microslice_size);
  ~dma_channel_pgen() = default;
  void set_sw_read_pointers(uint64_t data_offset,
                            uint64_t desc_offset) override;
  uint64_t get_desc_index() override { return m_desc_write_index; }
  size_t dma_transfer_size() const override { return 64; }

private:
  std::unique_ptr<RingBufferView<fles::MicrosliceDescriptor, false>>
      m_desc_buffer;
  std::unique_ptr<RingBufferView<uint8_t, false>> m_data_buffer;

  uint64_t m_desc_write_index = 0;
  uint64_t m_data_write_index = 0;
  uint64_t m_desc_read_index = 0;
  uint64_t m_data_read_index = 0;

  std::jthread m_worker_thread;
  void thread_work(std::stop_token stop_token);
};

} // namespace cri
