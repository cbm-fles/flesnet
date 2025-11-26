/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Dirk Hutter, Jan de Cuveland */

#include "Channel.hpp"
#include "MicrosliceDescriptor.hpp"
#include "log.hpp"
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
  uint64_t read_index = m_read_index;
  auto desc_begin = m_desc_buffer->get_iter(read_index);
  auto desc_end = m_desc_buffer->get_iter(write_index);

  // Find the first element with a time greater than requested time and deduce 1
  // to get the last element with a time <= requested time. We deduce the
  // overlap_before from the requested time to ensure next component can still
  // be built. This has to be the same logic as in find_component!
  time -= m_overlap_before_ns;
  auto it =
      std::upper_bound(desc_begin, desc_end, time,
                       [](uint64_t t, const fles::MicrosliceDescriptor& desc) {
                         return t < desc.idx;
                       });

  // To delete all elements before (aka time less than) the requested
  // time, we set the read index to the found element (the element the read
  // index points to is kept). If the requested time is greater than the last
  // valid element (write_index-1) the function returns desc_end (= write_index)
  // and we set read_index to write_index to clear everything. If the requested
  // time is less than the first element the function returns desc_begin (=
  // read_index). Setting the read index to desc_begin would not harm but we do
  // nothing to reduce strain on the hardware.
  if (it != desc_begin) {
    it--;
    set_read_index(it.get_index());
  }

  TRACE(
      "ack before: searching for time {} in range {} - {}. Setting read index "
      "to {}",
      time, desc_begin.get_index(), desc_end.get_index(), it.get_index());

  return;
}

Channel::State Channel::check_availability(uint64_t start_time,
                                           uint64_t duration) {

  uint64_t write_index = m_dma_channel->get_desc_index();
  uint64_t read_index = m_read_index;

  uint64_t first_ms_time = start_time - m_overlap_before_ns;
  uint64_t last_ms_time = start_time + duration + m_overlap_after_ns;

  TRACE("searching for component [{}, {}).", pt(first_ms_time),
        pt(last_ms_time));

  if (write_index == read_index) {
    TRACE("write and read index are equal, no data available");
    return Channel::State::TryLater;
  }
  if (first_ms_time < m_desc_buffer->at(read_index).idx) {
    // the first (oldest) microslice in the buffer is younger than the first
    // microslice we want, so we can never provide that component
    TRACE("Failed; begin want= {} have={}, difference={}", pt(first_ms_time),
          pt(m_desc_buffer->at(read_index).idx),
          int64_t(m_desc_buffer->at(read_index).idx - first_ms_time));
    return Channel::State::Failed;
  }
  if (last_ms_time >= m_desc_buffer->at(write_index - 1).idx) {
    // the last (youngest) microslice in the buffer is older than the last
    // microslice we want, so we can't provide that component yet
    TRACE("TryLater: end want={} have={}, difference={}", pt(last_ms_time),
          pt(m_desc_buffer->at(write_index - 1).idx),
          int64_t(last_ms_time - m_desc_buffer->at(write_index - 1).idx));
    return Channel::State::TryLater;
  }
  return Channel::State::Ok;
}

StComponentHandle Channel::get_descriptor(uint64_t start_time,
                                          uint64_t duration) {
  // find the component in the buffer
  auto [desc_begin_idx, desc_end_idx] = find_component(start_time, duration);
  assert(desc_begin_idx < desc_end_idx);

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

// Returns the component's microslice descriptor indexes in the range
// [start_time - ovelap_before, start_time + duration + overlap_after). Expects
// that the component is available. Check the component state before calling
// find_component.
std::pair<uint64_t, uint64_t> Channel::find_component(uint64_t start_time,
                                                      uint64_t duration) {
  uint64_t write_index = m_dma_channel->get_desc_index();
  uint64_t read_index = m_read_index;

  uint64_t first_ms_time = start_time - m_overlap_before_ns;
  uint64_t last_ms_time = start_time + duration + m_overlap_after_ns;

  auto desc_begin = m_desc_buffer->get_iter(read_index);
  auto desc_end = m_desc_buffer->get_iter(write_index);

  // We search for microslice in the range [first_ms_time, last_ms_time)
  // (we use the index from the first search to limit the second search)

  // search for begin iterator, i.e., the microslice before the first microslice
  // > time
  auto first_it =
      std::upper_bound(desc_begin, desc_end, first_ms_time,
                       [](uint64_t t, const fles::MicrosliceDescriptor& desc) {
                         return t < desc.idx;
                       });
  if (first_it == desc_begin || first_it == desc_end) {
    throw std::out_of_range("Component::find_component: beginning of "
                            "component out of range");
  }
  first_it--; // we want the first microslice <= time
  uint64_t first_idx = first_it.get_index();

  // search for the end iterator, i.e., the first microslice >= time
  auto last_it = std::lower_bound(first_it, desc_end, last_ms_time,
                                  [](const fles::MicrosliceDescriptor& desc,
                                     uint64_t t) { return desc.idx < t; });
  if (last_it == desc_begin || last_it == desc_end) {
    throw std::out_of_range(
        "Component::find_component: end of component out of range");
  }
  uint64_t last_idx = last_it.get_index();

  // INFO technically we are not allowed to dereference last_it because its ms
  // is not guaranteed to be written
  TRACE("find_component: want [{}, {}), have [{}, {}), diff {}, {}, idx [{}, "
        "{}), "
        "{} microslices",
        pt(first_ms_time), pt(last_ms_time), pt(first_it->idx),
        pt(last_it->idx), int64_t(first_it->idx - first_ms_time),
        int64_t(last_it->idx - last_ms_time), first_idx, last_idx,
        last_idx - first_idx);

  return {first_idx, last_idx};
}

void Channel::set_read_index(uint64_t read_index) {
  if (read_index == m_read_index) {
    TRACE("updating read_index, nothing to do for desc {}", read_index);
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

  TRACE("updating read_index: data {} desc {}", data_read_index, read_index);

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
