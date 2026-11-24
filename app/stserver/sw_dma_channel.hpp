/* Copyright (C) 2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "RingBufferView.hpp"
#include "dma_channel.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <span>
#include <stop_token>
#include <string>
#include <thread>

namespace cri {

/**
 * \brief A DMA channel emulated in software.
 *
 * A worker thread writes microslices into the descriptor and data ring buffers,
 * which the stserver Channel then reads exactly like buffers filled by a CRI.
 * The thread never blocks on the reader: a microslice that does not fit into
 * the buffers is dropped and counted, and the next microslice that fits
 * carries the OverflowFlim flag, like a FLIM that had to truncate.
 *
 * Derived classes implement thread_work(), call start() at the end of their
 * constructor and stop() at the beginning of their destructor.
 */
class sw_dma_channel : public basic_dma_channel {
public:
  sw_dma_channel(std::span<fles::MicrosliceDescriptor> desc_buffer,
                 std::span<uint8_t> data_buffer,
                 std::string name);
  ~sw_dma_channel() override = default;

  void set_sw_read_pointers(uint64_t data_offset,
                            uint64_t desc_offset) override;
  uint64_t get_desc_index() override;

  /// Number of microslices skipped because the buffers were full
  [[nodiscard]] uint64_t skipped_microslice_count() const {
    return m_skipped_microslice_count.load(std::memory_order_relaxed);
  }
  /// Content bytes of the microslices skipped because the buffers were full
  [[nodiscard]] uint64_t skipped_byte_count() const {
    return m_skipped_byte_count.load(std::memory_order_relaxed);
  }
  /// The name of the channel, used for monitoring and as thread name
  [[nodiscard]] const std::string& name() const { return m_name; }

protected:
  /// Start the worker thread
  void start();
  /// Request the worker thread to stop and wait for it
  void stop();
  /// The work of the worker thread, returns when a stop is requested
  virtual void thread_work(std::stop_token stop_token) = 0;

  /// Whether a microslice with the given content size fits into the buffers
  [[nodiscard]] bool fits(std::size_t content_bytes) const;

  /// Account for a microslice dropped because it does not fit
  void skip(std::size_t content_bytes);

  /**
   * \brief Append a microslice to the buffers. Expects fits(desc.size).
   *
   * `fill(dst, offset, n)` is called for each contiguous piece of the content
   * in the data buffer (two pieces when the content wraps around the end of
   * the buffer), with `offset` the position of the piece in the content. The
   * descriptor is appended afterwards, so `fill` may still update it (e.g.,
   * set its crc). The offset of the descriptor is set here, and the
   * OverflowFlim flag is added after a skipped microslice.
   */
  template <class Fill>
  void append(fles::MicrosliceDescriptor& desc, Fill&& fill) {
    const std::size_t content_bytes = desc.size;
    const std::size_t position = m_data_buffer.offset_bytes(m_data_write_index);
    const std::size_t first_piece =
        std::min(content_bytes, m_data_buffer.bytes() - position);
    fill(&m_data_buffer.at(m_data_write_index), std::size_t{0}, first_piece);
    if (first_piece < content_bytes) {
      fill(m_data_buffer.ptr(), first_piece, content_bytes - first_piece);
    }
    desc.offset = m_data_write_index;
    if (m_skipped) {
      // TODO: introduce and use a dedicated flag for skipped microslices
      desc.flags |= static_cast<uint16_t>(fles::MicrosliceFlags::OverflowFlim);
      m_skipped = false;
    }
    m_data_write_index += content_bytes;

    const uint64_t desc_write_index =
        m_desc_write_index.load(std::memory_order_relaxed);
    m_desc_buffer.at(desc_write_index) = desc;
    // publish the microslice to the reader
    m_desc_write_index.store(desc_write_index + 1, std::memory_order_release);
  }

  /// Append a microslice with the given content. Expects fits(desc.size).
  void append(fles::MicrosliceDescriptor desc, const uint8_t* content) {
    append(desc, [content](uint8_t* dst, std::size_t offset, std::size_t n) {
      std::memcpy(dst, content + offset, n);
    });
  }

private:
  RingBufferView<fles::MicrosliceDescriptor, false> m_desc_buffer;
  RingBufferView<uint8_t, false> m_data_buffer;
  std::string m_name;

  // Written by the worker thread, read by the stserver thread
  std::atomic<uint64_t> m_desc_write_index = 0;
  uint64_t m_data_write_index = 0; // worker thread only
  // Written by the stserver thread, read by the worker thread
  std::atomic<uint64_t> m_desc_read_index = 0;
  std::atomic<uint64_t> m_data_read_index = 0;
  uint64_t m_desc_offset = 0; // stserver thread only
  uint64_t m_data_offset = 0; // stserver thread only

  bool m_skipped = false; // worker thread only
  std::atomic<uint64_t> m_skipped_microslice_count = 0;
  std::atomic<uint64_t> m_skipped_byte_count = 0;

  std::jthread m_worker_thread;
};

} // namespace cri
