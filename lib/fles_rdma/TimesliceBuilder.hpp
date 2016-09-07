// Copyright 2013, 2016 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeConnection.hpp"
#include "IBConnectionGroup.hpp"
#include "RingBuffer.hpp"
#include "TimesliceBuffer.hpp"
#include <csignal>

/// Timeslice receiver and input node connection container class.
/** A TimesliceBuilder object represents a group of timeslice building
 connections to input nodes and receives timeslices to a timeslice buffer. */

class TimesliceBuilder : public IBConnectionGroup<ComputeNodeConnection>
{
public:
    /// The TimesliceBuilder constructor.
    TimesliceBuilder(uint64_t compute_index, TimesliceBuffer& timeslice_buffer,
                     unsigned short service, uint32_t num_input_nodes,
                     uint32_t timeslice_size,
                     volatile sig_atomic_t* signal_status, bool drop);

    TimesliceBuilder(const TimesliceBuilder&) = delete;
    void operator=(const TimesliceBuilder&) = delete;

    /// The TimesliceBuilder destructor.
    ~TimesliceBuilder();

    void report_status();

    void request_abort();

    virtual void operator()() override;

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
    virtual void on_connect_request(struct rdma_cm_event* event) override;

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) override;

    void poll_ts_completion();

private:
    uint64_t compute_index_;
    TimesliceBuffer& timeslice_buffer_;

    unsigned short service_;
    uint32_t num_input_nodes_;

    uint32_t timeslice_size_;

    size_t red_lantern_ = 0;
    uint64_t completely_written_ = 0;
    uint64_t acked_ = 0;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t, true> ack_;

    volatile sig_atomic_t* signal_status_;
    bool drop_;
};
