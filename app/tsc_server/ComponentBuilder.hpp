// Copyright 2025 Dirk Hutter
#pragma once

#include "DualRingBuffer.hpp"
#include "SubTimesliceDescriptor.hpp"
#include "cri_channel.hpp"
#include "cri_source.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdint>
#include <sys/types.h>

namespace ip = boost::interprocess;

class ComponentBuilder {

public:
  ComponentBuilder(ip::managed_shared_memory* shm,
                   cri::cri_channel* cri_channel,
                   size_t data_buffer_size_exp,
                   size_t desc_buffer_size_exp,
                   uint64_t time_overlap_before,
                   uint64_t time_overlap_after);

  ~ComponentBuilder();

  enum class ComponentState {
    Ok,       // component is available
    TryLater, // component is not available yet
    Failed    // component is too old and not available anymore
  };

  // TODO: remove this function, it is only for debugging
  void proceed();

  void ack_before(uint64_t time);

  ComponentState check_component(uint64_t start_time, uint64_t duration);

  fles::SubTimesliceComponentDescriptor get_component(uint64_t start_time,
                                                      uint64_t duration);

  std::pair<uint64_t, uint64_t>
  find_component(uint64_t start_time, uint64_t duration); // TODO: make private

private:
  void* alloc_buffer(size_t size_exp, size_t item_size);

  ip::managed_shared_memory* m_shm;
  cri::cri_channel* m_cri_channel;

  std::unique_ptr<cri_source> m_cri_source_buffer;

  uint64_t m_time_overlap_before;
  uint64_t m_time_overlap_after;
};
