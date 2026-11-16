/* Copyright (C) 2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#pragma once

#include "MicrosliceSource.hpp"
#include "StorableMicroslice.hpp"
#include "sw_dma_channel.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace cri {

/// The replay settings from the command line
struct ReplayParameters {
  /// Locators of the microslice streams, one channel per stream
  std::vector<std::string> inputs;
  /// Shift the timestamps so that the streams start at the current data time
  bool shift = false;
  /// Content bytes to preload per channel and replay in a loop (0: one pass)
  std::size_t loop_bytes = 0;
};

/**
 * \brief A channel replaying a microslice stream.
 *
 * The microslices are read from a MicrosliceSource (usually a .msa file) and
 * written into the buffers when the data clock reaches their start time, so
 * that the stream arrives at the rate it was recorded or simulated with. The
 * data clock of the chain has to match the timestamps of the stream, unless
 * they are shifted:
 *
 * With `start_time_ns` given, the timestamps are shifted by a whole number of
 * timeslices, so that the stream starts within the timeslice beginning at
 * `start_time_ns` and every microslice keeps its position within its
 * timeslice.
 *
 * With `loop_bytes` given, that many bytes of content are loaded into memory
 * first and then replayed again and again, every cycle shifted by the whole
 * timeslices the loaded segment spans. Looping needs shifted timestamps. Note
 * that detector data whose content encodes absolute time (e.g., epoch
 * messages) is no longer consistent with the shifted descriptors.
 */
class replay_channel : public sw_dma_channel {
public:
  replay_channel(std::span<fles::MicrosliceDescriptor> desc_buffer,
                 std::span<uint8_t> data_buffer,
                 std::unique_ptr<fles::MicrosliceSource> source,
                 uint64_t timeslice_duration_ns,
                 std::optional<uint64_t> start_time_ns,
                 std::size_t loop_bytes,
                 std::string name);
  ~replay_channel() override { stop(); }

  /// Whether the single pass over the stream is complete, i.e., every
  /// microslice has been written to the buffers or skipped
  [[nodiscard]] bool finished() const {
    return m_finished.load(std::memory_order_acquire);
  }
  /// Number of microslices written to the buffers
  [[nodiscard]] uint64_t microslice_count() const {
    return m_microslice_count.load(std::memory_order_relaxed);
  }

private:
  std::unique_ptr<fles::MicrosliceSource> m_source;
  uint64_t m_timeslice_duration_ns;
  std::optional<uint64_t> m_start_time_ns;
  std::size_t m_loop_bytes;

  std::atomic<bool> m_finished = false;
  std::atomic<uint64_t> m_microslice_count = 0;

  void thread_work(std::stop_token stop_token) override;
  void replay_stream(const std::stop_token& stop_token);
  void replay_loop(const std::stop_token& stop_token);

  /// The shift that moves the stream start into the start timeslice
  [[nodiscard]] int64_t shift_for(uint64_t first_idx) const;
  /// Wait until the data clock reaches the given time; false on stop request
  [[nodiscard]] bool wait_until(uint64_t data_time_ns,
                                const std::stop_token& stop_token) const;
  /// Write a microslice with its start time shifted
  void write(const fles::Microslice& ms, int64_t shift_ns);
};

} // namespace cri
