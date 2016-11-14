// Copyright 2012-2013, 2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "DualRingBuffer.hpp"
#include "RingBuffer.hpp"
#include <csignal>
#include <zmq.h>

/// Input buffer and compute node connection container class.
/** An ComponentSenderZeromq object represents an input buffer (filled by a
    FLIB) and a group of timeslice building connections to compute
    nodes. */

class ComponentSenderZeromq
{
public:
    /// The ComponentSenderZeromq default constructor.
    ComponentSenderZeromq(uint64_t input_index,
                          InputBufferReadInterface& data_source,
                          std::string listen_address, uint32_t timeslice_size,
                          uint32_t overlap_size, uint32_t max_timeslice_number,
                          volatile sig_atomic_t* signal_status);

    ComponentSenderZeromq(const ComponentSenderZeromq&) = delete;
    void operator=(const ComponentSenderZeromq&) = delete;

    /// The ComponentSenderZeromq default destructor.
    ~ComponentSenderZeromq();

    /// The thread main function.
    void operator()();

    friend void free_ts(void* data, void* hint);

private:
    /// This component's index in the list of input components
    uint64_t input_index_;

    /// Data source (e.g., FLIB via shared memory).
    InputBufferReadInterface& data_source_;

    /// Constant size (in microslices) of a timeslice component.
    const uint32_t timeslice_size_;

    /// Constant overlap size (in microslices) of a timeslice component.
    const uint32_t overlap_size_;

    /// Number of timeslices after which this run shall end
    const uint32_t max_timeslice_number_;

    /// Pointer to global signal status variable
    volatile sig_atomic_t* signal_status_;

    /// ZeroMQ context.
    void* zmq_context_;

    /// ZeroMQ socket.
    void* socket_;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t, true> ack_;

    /// Number of acknowledged timeslices (times two - desc and data).
    uint64_t acked_ts2_ = 0;

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
