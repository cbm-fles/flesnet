// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeConnection.hpp"
#include "IBConnectionGroup.hpp"
#include "RingBuffer.hpp"
#include "TimesliceComponentDescriptor.hpp"

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <csignal>

/// Compute buffer and input node connection container class.
/** A ComputeBuffer object represents a timeslice buffer (filled by
    the input nodes) and a group of timeslice building connections to
    input nodes. */

class ComputeBuffer : public IBConnectionGroup<ComputeNodeConnection>
{
public:
    /// The ComputeBuffer constructor.
    ComputeBuffer(uint64_t compute_index, uint32_t data_buffer_size_exp,
                  uint32_t desc_buffer_size_exp, unsigned short service,
                  uint32_t num_input_nodes, uint32_t timeslice_size,
                  uint32_t processor_instances,
                  const std::string processor_executable,
                  volatile sig_atomic_t* signal_status, std::string input_node_name_);

    ComputeBuffer(const ComputeBuffer&) = delete;
    void operator=(const ComputeBuffer&) = delete;

    /// The ComputeBuffer destructor.
    ~ComputeBuffer();

    void start_processes();

    void report_status();

    void request_abort();

    virtual void operator()() override;

    uint8_t* get_data_ptr(uint_fast16_t index);

    fles::TimesliceComponentDescriptor* get_desc_ptr(uint_fast16_t index);

    uint8_t& get_data(uint_fast16_t index, uint64_t offset);

    fles::TimesliceComponentDescriptor& get_desc(uint_fast16_t index,
                                                 uint64_t offset);

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
    virtual void on_connect_request(struct rdma_cm_event* event) override;

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) override;

    void poll_ts_completion();

private:
    uint64_t compute_index_;

    uint32_t data_buffer_size_exp_;
    uint32_t desc_buffer_size_exp_;

    unsigned short service_;
    uint32_t num_input_nodes_;

    uint32_t timeslice_size_;

    uint32_t processor_instances_;
    const std::string processor_executable_;
    std::string shared_memory_identifier_;

    size_t red_lantern_ = 0;
    uint64_t completely_written_ = 0;
    uint64_t acked_ = 0;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t, true> ack_;

    std::unique_ptr<boost::interprocess::shared_memory_object> data_shm_;
    std::unique_ptr<boost::interprocess::shared_memory_object> desc_shm_;

    std::unique_ptr<boost::interprocess::mapped_region> data_region_;
    std::unique_ptr<boost::interprocess::mapped_region> desc_region_;

    std::unique_ptr<boost::interprocess::message_queue> work_items_mq_;
    std::unique_ptr<boost::interprocess::message_queue> completions_mq_;

    volatile sig_atomic_t* signal_status_;
};
