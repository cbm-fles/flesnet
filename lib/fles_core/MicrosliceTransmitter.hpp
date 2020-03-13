// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::MicrosliceTransmitter class.
#pragma once

#include "DualRingBuffer.hpp"
#include "Microslice.hpp"
#include "Sink.hpp"

namespace fles {

/**
 * \brief The MicrosliceTransmitter class implements a mechanism to transmit
 * Microslices to an InputBufferWriteInterface object.
 */
class MicrosliceTransmitter : public MicrosliceSink {
public:
  /// Construct Microslice Transmitter connected to a given data sink.
  explicit MicrosliceTransmitter(InputBufferWriteInterface& data_sink);

  /// Delete copy constructor (non-copyable).
  MicrosliceTransmitter(const MicrosliceTransmitter&) = delete;
  /// Delete assignment operator (non-copyable).
  void operator=(const MicrosliceTransmitter&) = delete;

  ~MicrosliceTransmitter() override = default;

  /**
   * \brief Transmit the next item.
   *
   * This function blocks if there is not enough space available.
   */
  void put(std::shared_ptr<const Microslice> item) override;

  void end_stream() override { data_sink_.set_eof(true); }

private:
  bool try_put(const std::shared_ptr<const Microslice>& item);

  /// Data sink (e.g., shared memory buffer).
  InputBufferWriteInterface& data_sink_;

  DualIndex write_index_ = {0, 0};
  DualIndex read_index_cached_ = {0, 0};
};
} // namespace fles
