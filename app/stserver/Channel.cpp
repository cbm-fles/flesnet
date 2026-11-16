/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Dirk Hutter, Jan de Cuveland */

#include "Channel.hpp"
#include "MicrosliceDescriptor.hpp"
#include "log_short.hpp"
#include <chrono>
#include <sstream>

namespace {
[[maybe_unused]] std::string pt(uint64_t time_ns) {
  // Chrono time_point from nanoseconds
  std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>
      tp{std::chrono::nanoseconds{time_ns}};
  // Convert to time_t and remaining nanoseconds
  auto tp_seconds = std::chrono::time_point_cast<std::chrono::seconds>(tp);
  std::time_t t = std::chrono::system_clock::to_time_t(tp_seconds);
  auto rem_ns = (tp.time_since_epoch() - std::chrono::seconds{t}).count();

  std::tm tm = *std::localtime(&t);
  std::stringstream ss;
  ss.imbue(std::locale("")); // use locale for thousands separator
  ss << std::put_time(&tm, "%S") << "." << std::setfill('0') << std::setw(11)
     << rem_ns;
  // Bug here: 25.0000100,000 because setfill does not give thousands separators
  return ss.str();
}
} // namespace

Channel::Channel(cri::basic_dma_channel* dma_channel,
                 std::span<fles::MicrosliceDescriptor> desc_buffer,
                 std::span<uint8_t> data_buffer,
                 uint64_t overlap_before_ns,
                 uint64_t overlap_after_ns,
                 std::string name)
    : m_dma_channel(dma_channel), m_overlap_before_ns(overlap_before_ns),
      m_overlap_after_ns(overlap_after_ns), m_name(std::move(name)) {

  // initialize buffer interface
  m_desc_buffer =
      std::make_unique<RingBufferView<fles::MicrosliceDescriptor, false>>(
          desc_buffer.data(), desc_buffer.size());
  m_data_buffer = std::make_unique<RingBufferView<uint8_t, false>>(
      data_buffer.data(), data_buffer.size());
}

void Channel::ack_before(uint64_t time) {
  // TODO: we may want to introduce a cached write index as member of component
  // and rely on other call to update it from HW
  uint64_t write_index = m_dma_channel->get_desc_index();
  auto desc_begin = m_desc_buffer->get_iter(m_read_index);
  auto desc_end = m_desc_buffer->get_iter(write_index);

  // Release everything before the microslice that covers the overlap before
  // the requested time, so that the component starting there can still be
  // built. The element the read index points to is kept. If nothing can be
  // released, the hardware is left alone.
  const uint64_t keep_time =
      time > m_overlap_before_ns ? time - m_overlap_before_ns : 0;
  auto keep = fles::component_release_point(desc_begin, desc_end, keep_time);
  if (keep != desc_begin) {
    set_read_index(keep.get_index());
  }

  TRACE("{}: ack before: searching for time {} in range {} - {}. Setting read "
        "index to {}",
        m_name, pt(keep_time), desc_begin.get_index(), desc_end.get_index(),
        keep.get_index());
}

Channel::State Channel::check_availability(uint64_t start_time,
                                           uint64_t duration) {
  uint64_t write_index = m_dma_channel->get_desc_index();
  auto desc_begin = m_desc_buffer->get_iter(m_read_index);
  auto desc_end = m_desc_buffer->get_iter(write_index);
  auto w = window(start_time, duration);

  TRACE("{}: searching for component [{}, {}).", m_name, pt(w.begin_ns),
        pt(w.end_ns));

  auto state = fles::component_availability(desc_begin, desc_end, w);
  switch (state) {
  case State::Ok:
    break;
  case State::TryLater:
    if (desc_begin == desc_end) {
      TRACE("{}: write and read index are equal, no data available", m_name);
    } else {
      TRACE("{}: TryLater: end want={} have={}, difference={}", m_name,
            pt(w.end_ns), pt(std::prev(desc_end)->idx),
            int64_t(w.end_ns - std::prev(desc_end)->idx));
    }
    break;
  case State::Failed:
    TRACE("{}: Failed; begin want= {} have={}, difference={}", m_name,
          pt(w.begin_ns), pt(desc_begin->idx),
          int64_t(w.begin_ns - desc_begin->idx));
    break;
  }
  return state;
}

StComponentHandle Channel::get_descriptor(uint64_t start_time,
                                          uint64_t duration) {
  // find the component in the buffer; expects that the component is
  // available, check the state before calling get_descriptor
  auto [first, last] = fles::component_range(
      m_desc_buffer->get_iter(m_read_index),
      m_desc_buffer->get_iter(m_dma_channel->get_desc_index()),
      window(start_time, duration));
  uint64_t desc_begin_idx = first.get_index();
  uint64_t desc_end_idx = last.get_index();
  assert(desc_begin_idx < desc_end_idx);
  TRACE("{}: component: want [{}, {}), have [{}, {}), idx [{}, {}), {} "
        "microslices",
        m_name, pt(window(start_time, duration).begin_ns),
        pt(window(start_time, duration).end_ns), pt(first->idx), pt(last->idx),
        desc_begin_idx, desc_end_idx, desc_end_idx - desc_begin_idx);

  std::size_t num_microslices = desc_end_idx - desc_begin_idx;
  std::vector<ucp_dt_iov> iovs;

  // generate one or two i/o vectors for the microslice descriptors
  auto* desc_begin = &m_desc_buffer->at(desc_begin_idx);
  auto* desc_end = &m_desc_buffer->at(desc_end_idx);
  if (desc_begin < desc_end) {
    // the descriptors are contiguous in memory, so we only create one iovec
    iovs.push_back({desc_begin, (desc_end - desc_begin) *
                                    sizeof(fles::MicrosliceDescriptor)});
  } else if (desc_begin > desc_end) {
    // the descriptors have wrapped around the end of the buffer
    auto* desc_buffer_begin = m_desc_buffer->ptr();
    auto* desc_buffer_end = m_desc_buffer->ptr() + m_desc_buffer->size();
    iovs.push_back({desc_begin, (desc_buffer_end - desc_begin) *
                                    sizeof(fles::MicrosliceDescriptor)});
    iovs.push_back({desc_buffer_begin, (desc_end - desc_buffer_begin) *
                                           sizeof(fles::MicrosliceDescriptor)});
  }

  // find the data blocks for the component
  auto data_begin_idx = desc_begin->offset;
  auto data_end_idx = (desc_end - 1)->offset + (desc_end - 1)->size;

  // generate one or two i/o vectors for the microslice contents
  auto* data_begin = &m_data_buffer->at(data_begin_idx);
  auto* data_end = &m_data_buffer->at(data_end_idx);
  if (data_begin < data_end) {
    // the data blocks are contiguous in memory, so we only create one iovec
    iovs.push_back({data_begin, (data_end - data_begin) * sizeof(uint8_t)});
  } else if (data_begin > data_end) {
    // the data blocks have wrapped around the end of the buffer
    auto* data_buffer_begin = m_data_buffer->ptr();
    auto* data_buffer_end = m_data_buffer->ptr() + m_data_buffer->size();
    iovs.push_back(
        {data_begin, (data_buffer_end - data_begin) * sizeof(uint8_t)});
    iovs.push_back(
        {data_buffer_begin, (data_end - data_buffer_begin) * sizeof(uint8_t)});
  }

  // TODO: this is a placeholder for any aggregated information about the
  // microslices in the component
  uint32_t flags = 0;
  for (auto it = m_desc_buffer->get_iter(desc_begin_idx);
       it < m_desc_buffer->get_iter(desc_end_idx); ++it) {
    if ((it->flags &
         static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim)) != 0) {
      flags |= static_cast<uint32_t>(TsComponentFlag::OverflowFlim);
      break;
    }
  }

  return {iovs, num_microslices, flags};
}

void Channel::set_read_index(uint64_t read_index) {
  if (read_index == m_read_index) {
    TRACE("{}: updating read_index, nothing to do for desc {}", m_name,
          read_index);
    return;
  }

  if (read_index < m_read_index) {
    std::stringstream ss;
    ss << "new read index desc " << read_index
       << " is smaller than the current read index desc " << m_read_index
       << ", this should not happen!";
    throw std::runtime_error(ss.str());
  }

  uint64_t data_read_index = m_desc_buffer->at(read_index - 1).offset +
                             m_desc_buffer->at(read_index - 1).size;

  TRACE("{}: updating read_index: data {} desc {}", m_name, data_read_index,
        read_index);

  uint64_t desc_offset = m_desc_buffer->offset_bytes(read_index);
  uint64_t data_offset = m_data_buffer->offset_bytes(data_read_index);

  m_dma_channel->set_sw_read_pointers(data_offset, desc_offset);

  m_read_index = read_index;
}

Channel::Monitoring Channel::get_monitoring() const {
  Monitoring state{};

  uint64_t write_index = m_dma_channel->get_desc_index();

  if (write_index == m_read_index) {
    return state;
  }

  const std::size_t desc_buffer_items = write_index - m_read_index;
  const std::size_t data_buffer_items =
      m_desc_buffer->at(write_index - 1).offset +
      m_desc_buffer->at(write_index - 1).size -
      m_desc_buffer->at(m_read_index).offset;

  state.desc_buffer_utilization =
      static_cast<float>(desc_buffer_items) / m_desc_buffer->size();
  state.data_buffer_utilization =
      static_cast<float>(data_buffer_items) / m_data_buffer->size();
  state.latest_microslice_time_ns = m_desc_buffer->at(write_index - 1).idx;

  return state;
}
