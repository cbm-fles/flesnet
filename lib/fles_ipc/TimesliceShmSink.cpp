/* Copyright (C) 2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "TimesliceShmSink.hpp"
#include "SubTimeslice.hpp"
#include "Timeslice.hpp"
#include "Utility.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <thread>
#include <utility>

namespace fles {

TimesliceShmSink::TimesliceShmSink(zmq::context_t& context,
                                   std::string shm_identifier,
                                   std::size_t buffer_size)
    : m_buffer(context, std::move(shm_identifier), buffer_size) {}

void TimesliceShmSink::put(std::shared_ptr<const Timeslice> timeslice) {
  handle_completions();

  const uint64_t id = timeslice->index();
  if (m_buffer.is_outstanding(id)) {
    throw std::runtime_error(
        std::format("timeslice {} is still in use in shared memory", id));
  }

  tsb::StDescriptor desc;
  desc.start_time_ns = timeslice->start_time();
  desc.duration_ns = timeslice->duration();
  desc.flags = timeslice->flags();
  uint64_t size = 0;
  for (uint64_t c = 0; c < timeslice->num_components(); ++c) {
    tsb::StComponentDescriptor& component = desc.components.emplace_back();
    component.ms_data_offset = static_cast<std::ptrdiff_t>(size);
    component.ms_data_size = timeslice->size_component(c);
    component.num_microslices = timeslice->num_microslices(c);
    component.flags = timeslice->desc_ptr_[c]->flags;
    size += component.ms_data_size;
  }

  if (size > m_buffer.get_size()) {
    throw std::runtime_error(std::format(
        "timeslice {} of size {} does not fit into shared memory of size {}",
        id, human_readable_count(size, true),
        human_readable_count(m_buffer.get_size(), true)));
  }

  // Wait for timeslices to be released until the allocation succeeds
  std::byte* buffer = nullptr;
  while ((buffer = m_buffer.allocate(std::max<uint64_t>(size, 1))) == nullptr) {
    if (m_buffer.empty()) {
      throw std::runtime_error(std::format(
          "failed to allocate {} for timeslice {} in empty shared memory",
          human_readable_count(size, true), id));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    handle_completions();
  }

  for (uint64_t c = 0; c < timeslice->num_components(); ++c) {
    const auto& component = desc.components[c];
    std::memcpy(buffer + component.ms_data_offset, timeslice->data_ptr_[c],
                component.ms_data_size);
  }

  m_buffer.send_work_item(buffer, id, desc);
  m_allocations.emplace(id, buffer);
}

void TimesliceShmSink::handle_completions() {
  while (auto id = m_buffer.try_receive_completion()) {
    auto it = m_allocations.find(*id);
    if (it != m_allocations.end()) {
      m_buffer.deallocate(it->second);
      m_allocations.erase(it);
    }
  }
}

} // namespace fles
