/**
 * \file ComputeNodeConnection.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef COMPUTENODECONNECTION_HPP
#define COMPUTENODECONNECTION_HPP

#include <boost/thread.hpp>
#include "Infiniband.hpp"
#include "RingBuffer.hpp"
#include "Parameters.hpp"
#include "Timeslice.hpp"
#include "global.hpp"


/// Compute node connection class.
/** A ComputeNodeConnection object represents the endpoint of a single
    timeslice building connection from a compute node to an input
    node. */

class ComputeNodeConnection : public IBConnection
{
public:
    ComputeNodeConnection(struct rdma_event_channel* ec, uint_fast16_t index,
                          struct rdma_cm_id* id = nullptr) :
        IBConnection(ec, index, id),
        _data(par->cn_data_buffer_size_exp()),
        _desc(par->cn_desc_buffer_size_exp())
    {
    }

    /// Post a receive work request (WR) to the receive queue
    void post_recv_cn_wp() {
        if (out.beDebug()) {
            out.debug() << "[" << _index << "] "
                        << "POST RECEIVE _receive_cn_wp";
        }
        post_recv(&recv_wr);
    }

    void post_send_cn_ack() {
        if (out.beDebug()) {
            out.debug() << "[" << _index << "] "
                        << "POST SEND _send_cp_ack";
        }
        post_send(&send_wr);
    }

    void post_send_final_ack() {
        if (out.beDebug()) {
            out.debug() << "[" << _index << "] "
                        << "POST SEND FINAL ack";
        }
        send_wr.wr_id = ID_SEND_FINALIZE;
        post_send(&send_wr);
    }

    virtual void setup(struct ibv_pd* pd) {
        // register memory regions
        _mr_data = ibv_reg_mr(pd, _data.ptr(), _data.bytes(),
                              IBV_ACCESS_LOCAL_WRITE |
                              IBV_ACCESS_REMOTE_WRITE);
        _mr_desc = ibv_reg_mr(pd, _desc.ptr(), _desc.bytes(),
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
        recv_sge.addr = (uintptr_t) &_recv_cn_wp;
        recv_sge.length = sizeof(ComputeNodeBufferPosition);
        recv_sge.lkey = _mr_recv->lkey;

        recv_wr.wr_id = ID_RECEIVE_CN_WP | (_index << 8);
        recv_wr.sg_list = &recv_sge;
        recv_wr.num_sge = 1;

        send_sge.addr = (uintptr_t) &_send_cn_ack;
        send_sge.length = sizeof(ComputeNodeBufferPosition);
        send_sge.lkey = _mr_send->lkey;

        send_wr.wr_id = ID_SEND_CN_ACK;
        send_wr.opcode = IBV_WR_SEND;
        send_wr.send_flags = IBV_SEND_SIGNALED;
        send_wr.sg_list = &send_sge;
        send_wr.num_sge = 1;

        // post initial receive request
        post_recv_cn_wp();
    }

    virtual void on_connect_request(struct rdma_cm_event* event,
                                    struct ibv_pd* pd, struct ibv_cq* cq) {
        assert(event->param.conn.private_data_len >= sizeof(InputNodeInfo));
        memcpy(&_remote_info, event->param.conn.private_data, sizeof(InputNodeInfo));

        _index = _remote_info.index;

        IBConnection::on_connect_request(event, pd, cq);
    }

    /// Connection handler function, called on successful connection.
    /**
       \param event RDMA connection manager event structure
    */
    virtual void on_established(struct rdma_cm_event* event) {
        IBConnection::on_established(event);

        out.debug() << "remote index: " << _remote_info.index;
    }

    virtual void on_disconnected() {
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

        IBConnection::on_disconnected();
    }

    void
    check_buffer(ComputeNodeBufferPosition ack, ComputeNodeBufferPosition wp,
                 TimesliceComponentDescriptor* desc, void* data)
    {
        int ts = wp.desc - ack.desc;
        out.debug() << "received " << ts << " timeslices";
        for (uint64_t dp = ack.desc; dp < wp.desc; dp++) {
            TimesliceComponentDescriptor tcd = desc[dp % (1 << par->cn_desc_buffer_size_exp())];
            out.debug() << "checking ts #" << tcd.ts_num;
        }
    }

    void on_complete_recv()
    {
        if (_recv_cn_wp.data == UINT64_MAX && _recv_cn_wp.desc == UINT64_MAX) {
            out.info() << "received FINAL pointer update";
            // send FINAL ack
            _send_cn_ack = _recv_cn_wp;
            post_send_final_ack();
            return;
        }
                
        // post new receive request
        post_recv_cn_wp();
        _cn_wp = _recv_cn_wp;
        // debug output
        out.debug() << "RECEIVE _cn_wp: data=" << _cn_wp.data
                    << " desc=" << _cn_wp.desc;

        // check buffer contents
        check_buffer(_cn_ack, _cn_wp, _desc.ptr(), _data.ptr());

        // DEBUG: empty the buffer
        _cn_ack = _cn_wp;

        // send ack
        _send_cn_ack = _cn_ack;
        post_send_cn_ack();
    }

    void on_complete_send_finalize() {
        _done = true;
    }

    ComputeNodeBufferPosition _send_cn_ack = {};
    ComputeNodeBufferPosition _cn_ack = {};

    ComputeNodeBufferPosition _recv_cn_wp = {};
    ComputeNodeBufferPosition _cn_wp = {};

    RingBuffer<uint64_t> _data;
    RingBuffer<TimesliceComponentDescriptor> _desc;

    struct ibv_mr* _mr_data = nullptr;
    struct ibv_mr* _mr_desc = nullptr;
    struct ibv_mr* _mr_send = nullptr;
    struct ibv_mr* _mr_recv = nullptr;

    virtual std::unique_ptr<std::vector<uint8_t>> get_private_data() {
        std::unique_ptr<std::vector<uint8_t> >
            private_data(new std::vector<uint8_t>(sizeof(ComputeNodeInfo)));

        ComputeNodeInfo* cn_info = reinterpret_cast<ComputeNodeInfo*>(private_data->data());
        cn_info->data.addr = (uintptr_t) _data.ptr();
        cn_info->data.rkey = _mr_data->rkey;
        cn_info->desc.addr = (uintptr_t) _desc.ptr();
        cn_info->desc.rkey = _mr_desc->rkey;
        cn_info->index = par->node_index();

        return private_data;
    }

private:

    /// Information on remote end.
    InputNodeInfo _remote_info = {};

    /// InfiniBand receive work request
    struct ibv_recv_wr recv_wr = {};

    /// Scatter/gather list entry for receive work request
    struct ibv_sge recv_sge;

    /// Infiniband send work request
    struct ibv_send_wr send_wr = {};

    /// Scatter/gather list entry for send work request
    struct ibv_sge send_sge;
};


#endif /* COMPUTENODECONNECTION_HPP */
