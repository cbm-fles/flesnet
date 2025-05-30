// Copyright 2025 Dirk Hutter, Jan de Cuveland
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "RingBufferView.hpp"
#include "SubTimesliceDescriptor.hpp"
#include "cri_channel.hpp"
#include "dma_channel.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdint>
#include <memory>

class Component {
public:
  Component(boost::interprocess::managed_shared_memory* shm,
            cri::cri_channel* cri_channel,
            size_t data_buffer_size,
            size_t desc_buffer_size,
            uint64_t overlap_before_ns,
            uint64_t overlap_after_ns);

  ~Component();

  enum class State {
    Ok,       // component is available
    TryLater, // component is not available yet
    Failed    // component is too old and not available anymore
  };

  void ack_before(uint64_t time);

  State check_availability(uint64_t start_time, uint64_t duration);

  fles::SubTimesliceComponentDescriptor get_descriptor(uint64_t start_time,
                                                       uint64_t duration);

private:
  boost::interprocess::managed_shared_memory* m_shm;
  cri::cri_channel* m_cri_channel;
  cri::dma_channel* m_dma_channel;

  uint64_t m_overlap_before_ns;
  uint64_t m_overlap_after_ns;

  std::unique_ptr<RingBufferView<fles::MicrosliceDescriptor, false>>
      m_desc_buffer;
  std::unique_ptr<RingBufferView<uint8_t, false>> m_data_buffer;

  // TODO: in case of reconnects this has to be initialized with the hw value
  uint64_t m_cached_read_index = 0; // INFO not actual hw value

  std::pair<uint64_t, uint64_t> find_component(uint64_t start_time,
                                               uint64_t duration);

  void set_read_index(uint64_t desc_read_index);
};
