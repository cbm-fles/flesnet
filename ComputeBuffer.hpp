#pragma once
/**
 * \file ComputeBuffer.hpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

/// Compute buffer and input node connection container class.
/** A ComputeBuffer object represents a timeslice buffer (filled by
    the input nodes) and a group of timeslice building connections to
    input nodes. */

class ComputeBuffer : public IBConnectionGroup<ComputeNodeConnection>
{
public:
    /// The ComputeBuffer default constructor.
    explicit ComputeBuffer(uint64_t compute_index);

    ComputeBuffer(const ComputeBuffer&) = delete;
    void operator=(const ComputeBuffer&) = delete;

    /// The ComputeBuffer destructor.
    ~ComputeBuffer();

    virtual void run() override;

    static void start_processor_task(int i);

    uint8_t* get_data_ptr(uint_fast16_t index);

    TimesliceComponentDescriptor* get_desc_ptr(uint_fast16_t index);

    uint8_t& get_data(uint_fast16_t index, uint64_t offset);

    TimesliceComponentDescriptor& get_desc(uint_fast16_t index,
                                           uint64_t offset);

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
    virtual void on_connect_request(struct rdma_cm_event* event) override;

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) override;

    virtual void handle_ts_completion();

private:
    uint64_t _compute_index;

    size_t _red_lantern = 0;
    uint64_t _completely_written = 0;
    uint64_t _acked = 0;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t, true> _ack;

    std::unique_ptr<boost::interprocess::shared_memory_object> _data_shm;
    std::unique_ptr<boost::interprocess::shared_memory_object> _desc_shm;

    std::unique_ptr<boost::interprocess::mapped_region> _data_region;
    std::unique_ptr<boost::interprocess::mapped_region> _desc_region;

    concurrent_queue<TimesliceWorkItem> _work_items;
    concurrent_queue<TimesliceCompletion> _completions;

    std::unique_ptr<boost::interprocess::message_queue> _work_items_mq;
    std::unique_ptr<boost::interprocess::message_queue> _completions_mq;
};
