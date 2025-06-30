// Copyright 2025 Dirk Hutter, Jan de Cuveland
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "RingBufferView.hpp"
#include "SubTimeslice.hpp"
#include "dma_channel.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

class Channel {
public:
  Channel(cri::basic_dma_channel* dma_channel,
          std::span<fles::MicrosliceDescriptor> desc_buffer,
          std::span<uint8_t> data_buffer,
          uint64_t overlap_before_ns,
          uint64_t overlap_after_ns,
          std::string name);

  enum class State {
    Ok,       // component is available
    TryLater, // component is not available yet
    Failed    // component is too old and not available anymore
  };

  void ack_before(uint64_t time);

  State check_availability(uint64_t start_time, uint64_t duration);

  StComponentHandle get_descriptor(uint64_t start_time, uint64_t duration);

  struct Monitoring {
    float desc_buffer_utilization{};
    float data_buffer_utilization{};
    std::optional<int64_t> latest_microslice_time_ns;
  };

  [[nodiscard]] Monitoring get_monitoring() const;
  [[nodiscard]] const std::string& name() const { return m_name; }

private:
  cri::basic_dma_channel* m_dma_channel;

  uint64_t m_overlap_before_ns;
  uint64_t m_overlap_after_ns;

  std::string m_name;

  std::unique_ptr<RingBufferView<fles::MicrosliceDescriptor, false>>
      m_desc_buffer;
  std::unique_ptr<RingBufferView<uint8_t, false>> m_data_buffer;

  uint64_t m_read_index = 0; // hardware value is also initialized to 0

  std::pair<uint64_t, uint64_t> find_component(uint64_t start_time,
                                               uint64_t duration);

  void set_read_index(uint64_t desc_read_index);
};
