#pragma once
/**
 * \file InputChannelSender.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "IBConnectionGroup.hpp"
#include "InputChannelConnection.hpp"
#include "DataSource.hpp"
#include "RingBuffer.hpp"

/// Input buffer and compute node connection container class.
/** An InputChannelSender object represents an input buffer (filled by a
    FLIB) and a group of timeslice building connections to compute
    nodes. */

class InputChannelSender : public IBConnectionGroup<InputChannelConnection>
{
public:
    /// The InputChannelSender default constructor.
    InputChannelSender(uint64_t input_index,
                       DataSource& data_source,
                       const std::vector<std::string>& compute_hostnames,
                       const std::vector<std::string>& compute_services,
                       uint32_t timeslice_size,
                       uint32_t overlap_size,
                       uint32_t max_timeslice_number);

    InputChannelSender(const InputChannelSender&) = delete;
    void operator=(const InputChannelSender&) = delete;

    /// The InputChannelSender default destructor.
    virtual ~InputChannelSender();

    virtual void run() override;

    /// The central loop for distributing timeslice data.
    void sender_loop();

    std::unique_ptr<InputChannelConnection>
    create_input_node_connection(uint_fast16_t index);

    /// Initiate connection requests to list of target hostnames.
    void connect();

private:
    /// Return target computation node for given timeslice.
    int target_cn_index(uint64_t timeslice);

    void dump_mr(struct ibv_mr* mr);

    virtual void on_addr_resolved(struct rdma_cm_id* id) override;

    /// Handle RDMA_CM_REJECTED event.
    virtual void on_rejected(struct rdma_cm_event* event) override;

    /// Return string describing buffer contents, suitable for debug output.
    std::string get_state_string();

    /// Create gather list for transmission of timeslice
    void post_send_data(uint64_t timeslice, int cn, uint64_t mc_offset,
                        uint64_t mc_length, uint64_t data_offset,
                        uint64_t data_length, uint64_t skip);

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) override;

    uint64_t _input_index;

    /// InfiniBand memory region descriptor for input data buffer.
    struct ibv_mr* _mr_data = nullptr;

    /// InfiniBand memory region descriptor for input descriptor buffer.
    struct ibv_mr* _mr_desc = nullptr;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t, true> _ack;

    /// Number of acknowledged MCs. Written to FLIB.
    uint64_t _acked_mc = 0;

    /// Number of acknowledged data bytes. Written to FLIB.
    uint64_t _acked_data = 0;

    /// Data source (e.g., FLIB).
    DataSource& _data_source;

    const std::vector<std::string>& _compute_hostnames;
    const std::vector<std::string>& _compute_services;

    const uint32_t _timeslice_size;
    const uint32_t _overlap_size;
    const uint32_t _max_timeslice_number;

    const uint64_t _min_acked_mc;
    const uint64_t _min_acked_data;

    uint64_t _cached_acked_data = 0;
    uint64_t _cached_acked_mc = 0;
};
