#pragma once
/**
 * \file ComputeNodeConnection.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

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
                          std::size_t data_bytes,
                          TimesliceComponentDescriptor* desc_ptr,
                          std::size_t desc_bytes) :
        IBConnection(ec, connection_index, remote_connection_index, id),
        _remote_info(remote_info),
        _data_ptr(data_ptr),
        _data_bytes(data_bytes),
        _desc_ptr(desc_ptr),
        _desc_bytes(desc_bytes)
    {
        // send and receive only single ComputeNodeBufferPosition struct
        _qp_cap.max_send_wr = 2; // one additional wr to avoid race (recv before send completion)
        _qp_cap.max_send_sge = 1;
        _qp_cap.max_recv_wr = 1;
        _qp_cap.max_recv_sge = 1;
    }

    ComputeNodeConnection(const ComputeNodeConnection&) = delete;
    void operator=(const ComputeNodeConnection&) = delete;

    /// Post a receive work request (WR) to the receive queue
    void post_recv_cn_wp() {
        if (out.beDebug()) {
            out.debug() << "[c" << _remote_index << "] "
                        << "[" << _index << "] " << "POST RECEIVE _receive_cn_wp";
        }
        post_recv(&recv_wr);
    }

    void post_send_cn_ack() {
        if (out.beDebug()) {
            out.debug()  << "[c" << _remote_index << "] "
                         << "[" << _index << "] " << "POST SEND _send_cn_ack"
                        << " (desc=" << _send_cn_ack.desc << ")";
        }
        while (_pending_send_requests >= _qp_cap.max_send_wr) {
            throw InfinibandException("Max number of pending send requests exceeded");
        }
        ++_pending_send_requests;
        post_send(&send_wr);
    }

    void post_send_final_ack() {
        send_wr.wr_id = ID_SEND_FINALIZE | (_index << 8);
        send_wr.send_flags = IBV_SEND_SIGNALED;
        post_send_cn_ack();
    }

    virtual void setup(struct ibv_pd* pd) {
        assert(_data_ptr && _desc_ptr && _data_bytes && _desc_bytes);

        // register memory regions
        _mr_data = ibv_reg_mr(pd, _data_ptr, _data_bytes,
                              IBV_ACCESS_LOCAL_WRITE |
                              IBV_ACCESS_REMOTE_WRITE);
        _mr_desc = ibv_reg_mr(pd, _desc_ptr, _desc_bytes,
                              IBV_ACCESS_LOCAL_WRITE |
                              IBV_ACCESS_REMOTE_WRITE);
        _mr_send = ibv_reg_mr(pd, &_send_cn_ack,
                              sizeof(ComputeNodeBufferPosition), 0);
        _mr_recv = ibv_reg_mr(pd, &_recv_cn_wp,
                              sizeof(ComputeNodeBufferPosition),
                              IBV_ACCESS_LOCAL_WRITE);

        if (!_mr_data || !_mr_desc || !_mr_recv || !_mr_send)
            throw InfinibandException("registration of memory region failed");

        // setup send and receive buffers
        recv_sge.addr = reinterpret_cast<uintptr_t>(&_recv_cn_wp);
        recv_sge.length = sizeof(ComputeNodeBufferPosition);
        recv_sge.lkey = _mr_recv->lkey;

        recv_wr.wr_id = ID_RECEIVE_CN_WP | (_index << 8);
        recv_wr.sg_list = &recv_sge;
        recv_wr.num_sge = 1;

        send_sge.addr = reinterpret_cast<uintptr_t>(&_send_cn_ack);
        send_sge.length = sizeof(ComputeNodeBufferPosition);
        send_sge.lkey = _mr_send->lkey;

        send_wr.wr_id = ID_SEND_CN_ACK | (_index << 8);
        send_wr.opcode = IBV_WR_SEND;
        send_wr.send_flags = IBV_SEND_SIGNALED;
        send_wr.sg_list = &send_sge;
        send_wr.num_sge = 1;

        // post initial receive request
        post_recv_cn_wp();
    }

    /// Connection handler function, called on successful connection.
    /**
       \param event RDMA connection manager event structure
    */
    virtual void on_established(struct rdma_cm_event* event) {
        IBConnection::on_established(event);

        out.debug() << "[c" << _remote_index << "] " << "remote index: " << _remote_info.index;
    }

    virtual void on_disconnected(struct rdma_cm_event* event) {
        disconnect();

        if (_mr_recv) {
            ibv_dereg_mr(_mr_recv);
            _mr_recv = nullptr;
        }

        if (_mr_send) {
            ibv_dereg_mr(_mr_send);
            _mr_send = nullptr;
        }

        if (_mr_desc) {
            ibv_dereg_mr(_mr_desc);
            _mr_desc = nullptr;
        }

        if (_mr_data) {
            ibv_dereg_mr(_mr_data);
            _mr_data = nullptr;
        }

        IBConnection::on_disconnected(event);
    }

    void inc_ack_pointers(uint64_t ack_pos) {
        std::unique_lock<std::mutex> lock(_cn_ack_mutex);
        _cn_ack.desc = ack_pos;

        const TimesliceComponentDescriptor& acked_ts =
            _desc_ptr[(ack_pos - 1) & ((1 << par->cn_desc_buffer_size_exp()) - 1)];

        _cn_ack.data = acked_ts.offset + acked_ts.size;
        if (_our_turn) {
            _our_turn = false;
            _send_cn_ack = _cn_ack;
            post_send_cn_ack();
        }
    }

    void on_complete_recv()
    {
        if (_recv_cn_wp == CN_WP_FINAL) {
            out.debug() << "[c" << _remote_index << "] "
                        << "[" << _index << "] " << "received FINAL pointer update";
            // send FINAL ack
            _send_cn_ack = CN_WP_FINAL;
            post_send_final_ack();
            return;
        }
        if (out.beDebug()) {
            out.debug() << "[c" << _remote_index << "] "
                        << "[" << _index << "] " << "COMPLETE RECEIVE _receive_cn_wp"
                        << " (desc=" << _recv_cn_wp.desc << ")";
        }
        _cn_wp = _recv_cn_wp;
        post_recv_cn_wp();
        {
            std::unique_lock<std::mutex> lock(_cn_ack_mutex);
            if (_cn_ack != _send_cn_ack) {
                _send_cn_ack = _cn_ack;
                post_send_cn_ack();
            } else
                _our_turn = true;
        }
    }

    void on_complete_send() {
        _pending_send_requests--;
    }

    void on_complete_send_finalize() {
        _done = true;
    }

    const ComputeNodeBufferPosition& cn_wp() const {
        return _cn_wp;
    }

    virtual std::unique_ptr<std::vector<uint8_t>> get_private_data() {
        assert(_data_ptr && _desc_ptr && _data_bytes && _desc_bytes);
        std::unique_ptr<std::vector<uint8_t> >
            private_data(new std::vector<uint8_t>(sizeof(ComputeNodeInfo)));

        ComputeNodeInfo* cn_info = reinterpret_cast<ComputeNodeInfo*>(private_data->data());
        cn_info->data.addr = reinterpret_cast<uintptr_t>(_data_ptr);
        cn_info->data.rkey = _mr_data->rkey;
        cn_info->desc.addr = reinterpret_cast<uintptr_t>(_desc_ptr);
        cn_info->desc.rkey = _mr_desc->rkey;
        cn_info->index = _remote_index;
        cn_info->data_buffer_size_exp = par->cn_data_buffer_size_exp();
        cn_info->desc_buffer_size_exp = par->cn_desc_buffer_size_exp();

        return private_data;
    }

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
    std::size_t _data_bytes = 0;

    TimesliceComponentDescriptor* _desc_ptr = nullptr;
    std::size_t _desc_bytes = 0;

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
