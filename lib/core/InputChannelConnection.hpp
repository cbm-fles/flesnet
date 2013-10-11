#pragma once
/**
 * \file InputChannelConnection.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "IBConnection.hpp"
#include "Timeslice.hpp"
#include <mutex>
#include <condition_variable>
#include <atomic>

/// Input node connection class.
/** An InputChannelConnection object represents the endpoint of a single
    timeslice building connection from an input node to a compute
    node. */

class InputChannelConnection : public IBConnection
{
public:
    /// The InputChannelConnection constructor.
    InputChannelConnection(struct rdma_event_channel* ec,
                           uint_fast16_t connection_index,
                           uint_fast16_t remote_connection_index,
                           unsigned int max_send_wr,
                           unsigned int max_pending_write_requests,
                           struct rdma_cm_id* id = nullptr);

    InputChannelConnection(const InputChannelConnection&) = delete;
    void operator=(const InputChannelConnection&) = delete;

    /// Wait until enough space is available at target compute node.
    void wait_for_buffer_space(uint64_t data_size, uint64_t desc_size);

    /// Send data and descriptors to compute node.
    void send_data(struct ibv_sge* sge, int num_sge, uint64_t timeslice,
                   uint64_t mc_length, uint64_t data_length, uint64_t skip);

    /// Increment target write pointers after data has been sent.
    void inc_write_pointers(uint64_t data_size, uint64_t desc_size);

    // Get number of bytes to skip in advance (to avoid buffer wrap)
    uint64_t skip_required(uint64_t data_size);

    ///
    void finalize();

    void on_complete_write();

    /// Handle Infiniband receive completion notification.
    void on_complete_recv();

    virtual void setup(struct ibv_pd* pd) override;

    /// Connection handler function, called on successful connection.
    /**
       \param event RDMA connection manager event structure
    */
    virtual void on_established(struct rdma_cm_event* event) override;

    void dereg_mr();

    virtual void on_rejected(struct rdma_cm_event* event) override;

    virtual void on_disconnected(struct rdma_cm_event* event) override;

    virtual std::unique_ptr<std::vector<uint8_t> > get_private_data() override;

private:
    /// Post a receive work request (WR) to the receive queue
    void post_recv_cn_ack();

    /// Post a send work request (WR) to the send queue
    void post_send_cn_wp();

    /// Flag, true if it is the input nodes's turn to send a pointer update.
    bool _our_turn = true;

    bool _finalize = false;

    /// Access information for memory regions on remote end.
    ComputeNodeInfo _remote_info = {};

    /// Local copy of acknowledged-by-CN pointers
    ComputeNodeBufferPosition _cn_ack = {};

    /// Receive buffer for acknowledged-by-CN pointers
    ComputeNodeBufferPosition _receive_cn_ack = {};

    /// Infiniband memory region descriptor for acknowledged-by-CN pointers
    struct ibv_mr* _mr_recv = nullptr;

    /// Mutex protecting access to acknowledged-by-CN pointers
    std::mutex _cn_ack_mutex;

    /// Condition variable for acknowledged-by-CN pointers
    std::condition_variable _cn_ack_cond;

    /// Local version of CN write pointers
    ComputeNodeBufferPosition _cn_wp = {};

    /// Send buffer for CN write pointers
    ComputeNodeBufferPosition _send_cn_wp = {};

    /// Infiniband memory region descriptor for CN write pointers
    struct ibv_mr* _mr_send = nullptr;

    /// Mutex protecting access to CN write pointers
    std::mutex _cn_wp_mutex;

    /// InfiniBand receive work request
    struct ibv_recv_wr recv_wr = {};

    /// Scatter/gather list entry for receive work request
    struct ibv_sge recv_sge;

    /// Infiniband send work request
    struct ibv_send_wr send_wr = {};

    /// Scatter/gather list entry for send work request
    struct ibv_sge send_sge;

    std::atomic_uint _pending_write_requests{0};

    unsigned int _max_pending_write_requests = 0;
};
