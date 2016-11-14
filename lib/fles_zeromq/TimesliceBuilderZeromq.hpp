// Copyright 2013, 2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ManagedRingBuffer.hpp"
#include "RingBuffer.hpp"
#include "TimesliceBuffer.hpp"
#include <cassert>
#include <csignal>
#include <vector>
#include <zmq.h>

/// Timeslice builder class.
/** A TimesliceBuilderZeromq object initiates connections to input nodes
 * and
 * receives
 * timeslices to a timeslice buffer. */

class TimesliceBuilderZeromq
{
public:
    /// The TimesliceBuilderZeromq constructor.
    TimesliceBuilderZeromq(
        uint64_t compute_index, TimesliceBuffer& timeslice_buffer,
        const std::vector<std::string> input_server_addresses,
        uint32_t num_compute_nodes, uint32_t timeslice_size,
        uint32_t max_timeslice_number, volatile sig_atomic_t* signal_status);

    TimesliceBuilderZeromq(const TimesliceBuilderZeromq&) = delete;
    void operator=(const TimesliceBuilderZeromq&) = delete;

    /// The TimesliceBuilderZeromq destructor.
    ~TimesliceBuilderZeromq();

    /// The thread main function.
    void operator()();

private:
    /// This builder's index in the list of compute nodes.
    const uint64_t compute_index_;

    /// Shared memory buffer to store received timeslices.
    TimesliceBuffer& timeslice_buffer_;

    /// Vector of all input server addresses to connect to.
    const std::vector<std::string> input_server_addresses_;

    /// Number of compute nodes.
    const uint32_t num_compute_nodes_;

    /// Constant size (in microslices) of a timeslice component.
    const uint32_t timeslice_size_;

    /// Number of timeslices after which this run shall end
    const uint32_t max_timeslice_number_;

    /// Pointer to global signal status variable
    volatile sig_atomic_t* signal_status_;

    /// ZeroMQ context.
    void* zmq_context_;

    /// Index of acknowledged timeslices (local index).
    uint64_t acked_ = 0;

    /// The global index of the timeslice currently being received.
    uint64_t ts_index_;

    /// The local buffer position of the timeslice currently being received.
    uint64_t tpos_ = 0;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t, true> ack_;

    /// Connection struct, which handles data for one input server.
    struct Connection {
        Connection(TimesliceBuffer& timeslice_buffer, size_t i)
            : desc(timeslice_buffer.get_desc_ptr(i),
                   timeslice_buffer.get_desc_size_exp()),
              data(timeslice_buffer.get_data_ptr(i),
                   timeslice_buffer.get_data_size_exp())
        {
        }

        ManagedRingBuffer<fles::TimesliceComponentDescriptor> desc;
        ManagedRingBuffer<uint8_t> data;

        void* socket;
        zmq_msg_t desc_msg;
        zmq_msg_t data_msg;
    };

    /// The vector of connections, one per input server.
    std::vector<std::unique_ptr<Connection>> connections_;

    /// Handle pending timeslice completions and advance read indexes.
    void handle_timeslice_completions();
};
