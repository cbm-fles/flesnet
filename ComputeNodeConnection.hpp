/**
 * \file ComputeNodeConnection.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef COMPUTENODECONNECTION_HPP
#define COMPUTENODECONNECTION_HPP

#include <boost/thread.hpp>
#include "Infiniband.hpp"
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
    ComputeNodeConnection(struct rdma_event_channel* ec, int index, struct rdma_cm_id* id = 0) :
        IBConnection(ec, index, id)
    {
        memset(&_send_cn_ack, 0, sizeof(ComputeNodeBufferPosition));
        memset(&_cn_ack, 0, sizeof(ComputeNodeBufferPosition));
        memset(&_cn_wp, 0, sizeof(ComputeNodeBufferPosition));
        memset(&_recv_cn_wp, 0, sizeof(ComputeNodeBufferPosition));

        // Allocate buffer space
        _data = (uint64_t*) calloc(Par->cn_data_buffer_size(), sizeof(uint64_t));
        _desc = (TimesliceComponentDescriptor*) calloc(Par->cn_desc_buffer_size(),
                                                       sizeof(TimesliceComponentDescriptor));
        if (!_data || !_desc)
            throw InfinibandException("allocation of buffer space failed");
    }

    virtual ~ComputeNodeConnection() {
        free(_desc);
        free(_data);
    }

    void post_receive() {
        struct ibv_sge sge;
        sge.addr = (uintptr_t) &_recv_cn_wp;
        sge.length = sizeof(ComputeNodeBufferPosition);
        sge.lkey = _mr_recv->lkey;
        struct ibv_recv_wr recv_wr;
        memset(&recv_wr, 0, sizeof recv_wr);
        recv_wr.wr_id = ID_RECEIVE_CN_WP;
        recv_wr.sg_list = &sge;
        recv_wr.num_sge = 1;
        post_recv(&recv_wr);
    }

    void send_ack(bool final = false) {
        _send_cn_ack = final ? _recv_cn_wp : _cn_ack;
        Log.debug() << "SEND posted";
        struct ibv_sge sge;
        sge.addr = (uintptr_t) &_send_cn_ack;
        sge.length = sizeof(ComputeNodeBufferPosition);
        sge.lkey = _mr_send->lkey;
        struct ibv_send_wr send_wr;
        memset(&send_wr, 0, sizeof send_wr);
        send_wr.wr_id = final ? ID_SEND_FINALIZE : ID_SEND_CN_ACK;
        send_wr.opcode = IBV_WR_SEND;
        send_wr.send_flags = IBV_SEND_SIGNALED;
        send_wr.sg_list = &sge;
        send_wr.num_sge = 1;
        post_send(&send_wr);
    }

    virtual void on_connect_request(struct ibv_pd* pd, struct ibv_cq* cq) {
        IBConnection::on_connect_request(pd, cq);
        
        // register memory regions
        _mr_data = ibv_reg_mr(pd, _data,
                              Par->cn_data_buffer_size() * sizeof(uint64_t),
                              IBV_ACCESS_LOCAL_WRITE |
                              IBV_ACCESS_REMOTE_WRITE);
        _mr_desc = ibv_reg_mr(pd, _desc,
                              Par->cn_desc_buffer_size() * sizeof(TimesliceComponentDescriptor),
                              IBV_ACCESS_LOCAL_WRITE |
                              IBV_ACCESS_REMOTE_WRITE);
        _mr_send = ibv_reg_mr(pd, &_send_cn_ack,
                              sizeof(ComputeNodeBufferPosition), 0);
        _mr_recv = ibv_reg_mr(pd, &_recv_cn_wp,
                              sizeof(ComputeNodeBufferPosition),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_data || !_mr_desc || !_mr_recv || !_mr_send)
            throw InfinibandException("registration of memory region failed");

        // post initial receive request
        post_receive();

        Log.debug() << "accepting connection";

        // Accept rdma connection request
        ServerInfo rep_pdata[2];
        rep_pdata[0].addr = (uintptr_t) _data;
        rep_pdata[0].rkey = _mr_data->rkey;
        rep_pdata[1].addr = (uintptr_t) _desc;
        rep_pdata[1].rkey = _mr_desc->rkey;
        
        struct rdma_conn_param conn_param;
        memset(&conn_param, 0, sizeof conn_param);
        conn_param.responder_resources = 1;
        conn_param.private_data = rep_pdata;
        conn_param.private_data_len = sizeof rep_pdata;
        int err = rdma_accept(_cm_id, &conn_param);
        if (err)
            throw InfinibandException("RDMA accept failed");
    }

    virtual void on_disconnect() {
        if (_mr_recv) {
            ibv_dereg_mr(_mr_recv);
            _mr_recv = 0;
        }

        if (_mr_send) {
            ibv_dereg_mr(_mr_send);
            _mr_send = 0;
        }

        if (_mr_desc) {
            ibv_dereg_mr(_mr_desc);
            _mr_desc = 0;
        }

        if (_mr_data) {
            ibv_dereg_mr(_mr_data);
            _mr_data = 0;
        }

        IBConnection::on_disconnect();
    }

    void
    check_buffer(ComputeNodeBufferPosition ack, ComputeNodeBufferPosition wp,
                TimesliceComponentDescriptor* desc, void *data)
    {
        int ts = wp.desc - ack.desc;
        Log.debug() << "received " << ts << " timeslices";
        for (uint64_t dp = ack.desc; dp < wp.desc; dp++) {
            TimesliceComponentDescriptor tcd = desc[dp % Par->cn_desc_buffer_size()];
            Log.debug() << "checking ts #" << tcd.ts_num;
        }
    }

    void on_complete_recv()
    {
        if (_recv_cn_wp.data == UINT64_MAX && _recv_cn_wp.desc == UINT64_MAX) {
            Log.info() << "received FINAL pointer update";
            // send FINAL ack
            send_ack(true);
            return;
        }
                
        // post new receive request
        post_receive();
        _cn_wp = _recv_cn_wp;
        // debug output
        Log.debug() << "RECEIVE _cn_wp: data=" << _cn_wp.data
                    << " desc=" << _cn_wp.desc;

        // check buffer contents
        check_buffer(_cn_ack, _cn_wp, _desc, _data);

        // DEBUG: empty the buffer
        _cn_ack = _cn_wp;

        // send ack
        send_ack();
    }

    ComputeNodeBufferPosition _send_cn_ack;
    ComputeNodeBufferPosition _cn_ack;

    ComputeNodeBufferPosition _recv_cn_wp;
    ComputeNodeBufferPosition _cn_wp;

    uint64_t* _data = nullptr;
    TimesliceComponentDescriptor* _desc = nullptr;

    struct ibv_mr* _mr_data = nullptr;
    struct ibv_mr* _mr_desc = nullptr;
    struct ibv_mr* _mr_send = nullptr;
    struct ibv_mr* _mr_recv = nullptr;

};


#endif /* COMPUTENODECONNECTION_HPP */
