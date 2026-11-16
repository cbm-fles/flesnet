/* Copyright (C) 2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "replay_channel.hpp"
#include "System.hpp"
#include "Utility.hpp"
#include "log_short.hpp"
#include <algorithm>
#include <chrono>
#include <utility>

using namespace std::chrono_literals;

namespace cri {

replay_channel::replay_channel(
    std::span<fles::MicrosliceDescriptor> desc_buffer,
    std::span<uint8_t> data_buffer,
    std::unique_ptr<fles::MicrosliceSource> source,
    uint64_t timeslice_duration_ns,
    std::optional<uint64_t> start_time_ns,
    std::size_t loop_bytes,
    std::string name)
    : sw_dma_channel(desc_buffer, data_buffer, std::move(name)),
      m_source(std::move(source)),
      m_timeslice_duration_ns(timeslice_duration_ns),
      m_start_time_ns(start_time_ns), m_loop_bytes(loop_bytes) {
  if (m_loop_bytes > 0 && !m_start_time_ns) {
    throw std::invalid_argument("replaying in a loop needs a start time");
  }
  start();
}

void replay_channel::thread_work(std::stop_token stop_token) {
  try {
    if (m_loop_bytes > 0) {
      replay_loop(stop_token);
    } else {
      replay_stream(stop_token);
    }
  } catch (std::exception const& e) {
    ERROR("{}: replay failed: {}", name(), e.what());
  }
  m_finished.store(true, std::memory_order_release);
}

int64_t replay_channel::shift_for(uint64_t first_idx) const {
  if (!m_start_time_ns) {
    return 0;
  }
  // move the timeslice containing the first microslice to the start timeslice
  const uint64_t first_timeslice_start =
      first_idx / m_timeslice_duration_ns * m_timeslice_duration_ns;
  return static_cast<int64_t>(*m_start_time_ns) -
         static_cast<int64_t>(first_timeslice_start);
}

bool replay_channel::wait_until(uint64_t data_time_ns,
                                const std::stop_token& stop_token) const {
  const auto& clock = fles::system::data_clock();
  while (!stop_token.stop_requested()) {
    const uint64_t now = fles::system::current_time_ns();
    if (now >= data_time_ns) {
      return true;
    }
    // bounded, so that a stop request is seen in time also when the data
    // clock runs slowly or has not started yet
    std::this_thread::sleep_for(
        std::min(clock.real_duration(static_cast<int64_t>(data_time_ns - now)),
                 std::chrono::nanoseconds(100ms)));
  }
  return false;
}

void replay_channel::write(const fles::Microslice& ms, int64_t shift_ns) {
  if (!fits(ms.desc().size)) {
    skip(ms.desc().size);
    return;
  }
  fles::MicrosliceDescriptor desc = ms.desc();
  desc.idx = static_cast<uint64_t>(static_cast<int64_t>(desc.idx) + shift_ns);
  append(desc, ms.content());
  m_microslice_count.fetch_add(1, std::memory_order_relaxed);
}

void replay_channel::replay_stream(const std::stop_token& stop_token) {
  std::optional<int64_t> shift;
  while (!stop_token.stop_requested()) {
    auto ms = m_source->get();
    if (!ms) {
      INFO("{}: end of stream after {} microslices", name(),
           microslice_count());
      return;
    }
    if (!shift) {
      shift = shift_for(ms->desc().idx);
      INFO("{}: replaying from {} (shifted by {} s)", name(),
           fles::system::format_time_point_ns(
               static_cast<int64_t>(ms->desc().idx) + *shift),
           *shift / 1e9);
      const int64_t start_ns = static_cast<int64_t>(ms->desc().idx) + *shift;
      const auto now_ns = static_cast<int64_t>(fles::system::current_time_ns());
      if (start_ns < now_ns) {
        WARN("{}: the stream starts {:.3f} s behind the data clock, its first "
             "timeslices will be lost; set the data time reference later",
             name(), static_cast<double>(now_ns - start_ns) / 1e9);
      }
    }
    if (!wait_until(ms->desc().idx + *shift, stop_token)) {
      return;
    }
    write(*ms, *shift);
  }
}

void replay_channel::replay_loop(const std::stop_token& stop_token) {
  // Load the segment
  std::vector<fles::StorableMicroslice> segment;
  std::size_t content_bytes = 0;
  while (content_bytes < m_loop_bytes && !stop_token.stop_requested()) {
    auto ms = m_source->get();
    if (!ms) {
      break;
    }
    content_bytes += ms->desc().size;
    segment.emplace_back(*ms);
  }
  if (segment.empty()) {
    throw std::runtime_error("no microslices to replay");
  }
  // The microslices in the buffers are those loaded, the source is done
  m_source.reset();

  // Every cycle advances the timestamps by the whole timeslices the segment
  // spans, so that the cycles tile the time axis without overlap
  const uint64_t first_idx = segment.front().desc().idx;
  const uint64_t last_idx = segment.back().desc().idx;
  const uint64_t period_ns = (last_idx / m_timeslice_duration_ns + 1 -
                              first_idx / m_timeslice_duration_ns) *
                             m_timeslice_duration_ns;
  const int64_t shift = shift_for(first_idx);
  INFO("{}: looping over {} microslices ({}, {} s) from {}", name(),
       segment.size(), human_readable_count(content_bytes, true),
       period_ns / 1e9,
       fles::system::format_time_point_ns(static_cast<int64_t>(first_idx) +
                                          shift));

  for (uint64_t cycle = 0;; ++cycle) {
    const int64_t cycle_shift = shift + static_cast<int64_t>(cycle * period_ns);
    for (const fles::Microslice& ms : segment) {
      if (!wait_until(ms.desc().idx + cycle_shift, stop_token)) {
        return;
      }
      write(ms, cycle_shift);
    }
  }
}

} // namespace cri
