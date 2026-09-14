/* Copyright (C) 2026 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
/// \file
/// \brief Defines the fles::TimesliceShmSink class.
#pragma once

#include "ItemProducer.hpp"
#include "Sink.hpp"
#include "TimesliceShmBuffer.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

namespace zmq {
class context_t;
}

namespace fles {

class Timeslice;

/**
 * \brief The TimesliceShmSink class copies timeslices into a shared memory
 * segment and publishes them to the connected workers.
 *
 * The layout in shared memory is the same as the one written by tsbuilder. If
 * the segment is full, put() blocks until enough timeslices have been
 * released by the workers.
 */
class TimesliceShmSink : public TimesliceSink {
public:
  /// Create the shared memory segment of the given size in bytes.
  TimesliceShmSink(zmq::context_t& context,
                   std::string shm_identifier,
                   std::size_t buffer_size);

  TimesliceShmSink(const TimesliceShmSink&) = delete;
  void operator=(const TimesliceShmSink&) = delete;

  ~TimesliceShmSink() override = default;

  void put(std::shared_ptr<const Timeslice> timeslice) override;

  /// Release the timeslices that are no longer in use.
  void handle_completions();

  /// Check whether no timeslice is in use (after handle_completions()).
  [[nodiscard]] bool empty() const { return m_buffer.empty(); }

private:
  TimesliceShmBuffer m_buffer;

  /// The buffer locations of the published timeslices
  std::unordered_map<ItemID, std::byte*> m_allocations;
};

} // namespace fles
