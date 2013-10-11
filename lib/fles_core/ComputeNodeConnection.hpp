#pragma once
/**
 * \file ComputeNodeConnection.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "IBConnection.hpp"
#include "Timeslice.hpp"
#include <mutex>
#include <atomic>

/// Compute node connection class.
/** A ComputeNodeConnection object represents the endpoint of a single
    timeslice building connection from a compute node to an input
    node. */

class ComputeNodeConnection : public IBConnection
{
public:
    ComputeNodeConnection(struct rdma_event_channel* ec,
                          uint_fast16_t connection_index,
                          uint_fast16_t remote_connection_index,
                          struct rdma_cm_id* id,
                          InputNodeInfo remote_info,
                          uint8_t* data_ptr,
                          uint32_t data_buffer_size_exp,
                          TimesliceComponentDescriptor* desc_ptr,
                          uint32_t desc_buffer_size_exp);

    ComputeNodeConnection(const ComputeNodeConnection&) = delete;
    void operator=(const ComputeNodeConnection&) = delete;

    /// Post a receive work request (WR) to the receive queue
    void post_recv_cn_wp();

    void post_send_cn_ack();

    void post_send_final_ack();

    virtual void setup(struct ibv_pd* pd) override;

    /// Connection handler function, called on successful connection.
    /**
       \param event RDMA connection manager event structure
    */
    virtual void on_established(struct rdma_cm_event* event) override;

    virtual void on_disconnected(struct rdma_cm_event* event) override;

    void inc_ack_pointers(uint64_t ack_pos);

    void on_complete_recv();

    void on_complete_send();

    void on_complete_send_finalize();

    const ComputeNodeBufferPosition& cn_wp() const
    {
        return _cn_wp;
    }

    virtual std::unique_ptr<std::vector<uint8_t> > get_private_data() override;

private:
    ComputeNodeBufferPosition _send_cn_ack = {};
    ComputeNodeBufferPosition _cn_ack = {};
    std::mutex _cn_ack_mutex;

    ComputeNodeBufferPosition _recv_cn_wp = {};
    ComputeNodeBufferPosition _cn_wp = {};

    struct ibv_mr* _mr_data = nullptr;
    struct ibv_mr* _mr_desc = nullptr;
    struct ibv_mr* _mr_send = nullptr;
    struct ibv_mr* _mr_recv = nullptr;

    /// Flag, true if it is the input nodes's turn to send a pointer update.
    bool _our_turn = false;

    /// Information on remote end.
    InputNodeInfo _remote_info = {};

    uint8_t* _data_ptr = nullptr;
    std::size_t _data_buffer_size_exp = 0;

    TimesliceComponentDescriptor* _desc_ptr = nullptr;
    std::size_t _desc_buffer_size_exp = 0;

    /// InfiniBand receive work request
    struct ibv_recv_wr recv_wr = {};

    /// Scatter/gather list entry for receive work request
    struct ibv_sge recv_sge;

    /// Infiniband send work request
    struct ibv_send_wr send_wr = {};

    /// Scatter/gather list entry for send work request
    struct ibv_sge send_sge;

    std::atomic_uint _pending_send_requests{0};
};
