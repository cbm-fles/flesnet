// Copyright 2012-2013, 2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "DualRingBuffer.hpp"
#include "RingBuffer.hpp"
#include <zmq.h>

/// Input buffer and compute node connection container class.
/** An ComponentSenderZeromq object represents an input buffer (filled by a
    FLIB) and a group of timeslice building connections to compute
    nodes. */

class ComponentSenderZeromq
{
public:
    /// The ComponentSenderZeromq default constructor.
    ComponentSenderZeromq(InputBufferReadInterface& data_source,
                          uint32_t timeslice_size, uint32_t overlap_size,
                          std::string listen_address);

    ComponentSenderZeromq(const ComponentSenderZeromq&) = delete;
    void operator=(const ComponentSenderZeromq&) = delete;

    /// The ComponentSenderZeromq default destructor.
    ~ComponentSenderZeromq();

    /// The thread main function.
    void operator()();

    friend void free_ts(void* data, void* hint);

private:
    /// Data source (e.g., FLIB via shared memory).
    InputBufferReadInterface& data_source_;

    /// Constant size (in microslices) of a timeslice component.
    const uint32_t timeslice_size_;

    /// Constant overlap size (in microslices) of a timeslice component.
    const uint32_t overlap_size_;

    /// ZeroMQ context.
    void* zmq_context_;

    /// ZeroMQ socket.
    void* socket_;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<DualIndex, true> ack_;

    /// Number of acknowledged timeslices.
    uint64_t acked_ts_ = 0;

    /// Indexes of acknowledged microslices (i.e., read indexes).
    DualIndex acked_;

    /// Hysteresis for writing read indexes to data source.
    const DualIndex min_acked_;

    /// Read indexes last written to data source.
    DualIndex cached_acked_;

    /// Read indexes at start of operation.
    DualIndex start_index_;

    /// Write index received from data source.
    uint64_t write_index_desc_ = 0;

    /// The central function for distributing timeslice data.
    bool try_send_timeslice(uint64_t timeslice);

    /// Create zeromq message part with requested data
    template <typename T_>
    zmq_msg_t create_message(RingBufferView<T_>& buf, uint64_t offset,
                             uint64_t length, uint64_t ts, bool is_data);

    /// Update read indexes after timeslice has been sent.
    void ack_timeslice(uint64_t ts, bool is_data);

    /// Force writing read indexes to data source.
    void sync_data_source();
};
