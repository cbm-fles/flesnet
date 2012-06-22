/**
 * \file InputNodeConnection.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef INPUTNODECONNECTION_HPP
#define INPUTNODECONNECTION_HPP

#include <boost/thread.hpp>
#include "Infiniband.hpp"
#include "Parameters.hpp"
#include "Timeslice.hpp"
#include "global.hpp"


/// Input node connection class.
/** A InputNodeConnection object represents the endpoint of a single
    timeslice building connection from a compute node to an input
    node. */

class InputNodeConnection : public IBConnection
{
public:

    /// The InputNodeConnection constructor.
    InputNodeConnection(struct rdma_event_channel* ec, int index) :
        IBConnection(ec, index),
        _ourTurn(true)
    {
        _qp_cap.max_send_wr = 20;
        _qp_cap.max_send_sge = 8;
        _qp_cap.max_recv_wr = 20;
        _qp_cap.max_recv_sge = 8;
        _qp_cap.max_inline_data = sizeof(TimesliceComponentDescriptor) * 10;

        memset(&_receive_cn_ack, 0, sizeof(InputNodeBufferPosition));
        memset(&_cn_ack, 0, sizeof(InputNodeBufferPosition));
        memset(&_cn_wp, 0, sizeof(InputNodeBufferPosition));
        memset(&_send_cn_wp, 0, sizeof(InputNodeBufferPosition));
    }

    /// Handle Infiniband receive completion notification
    void onCompleteRecv() {
    }

    /// Handle RDMA_CM_EVENT_ADDR_RESOLVED event for this connection.
    virtual void onAddrResolved(struct ibv_pd* pd, struct ibv_cq* cq) {
        IBConnection::onAddrResolved(pd, cq);
        
        // register memory regions
        _mr_recv = ibv_reg_mr(pd, &_receive_cn_ack,
                              sizeof(InputNodeBufferPosition),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_recv)
            throw InfinibandException("registration of memory region failed");

        _mr_send = ibv_reg_mr(pd, &_send_cn_wp,
                              sizeof(InputNodeBufferPosition), 0);
        if (!_mr_send)
            throw InfinibandException("registration of memory region failed");

        // setup send and receive buffers
        recv_sge.addr = (uintptr_t) &_receive_cn_ack;
        recv_sge.length = sizeof(InputNodeBufferPosition);
        recv_sge.lkey = _mr_recv->lkey;
        memset(&recv_wr, 0, sizeof recv_wr);
        recv_wr.wr_id = ID_RECEIVE_CN_ACK | (_index << 8);
        recv_wr.sg_list = &recv_sge;
        recv_wr.num_sge = 1;

        send_sge.addr = (uintptr_t) &_send_cn_wp;
        send_sge.length = sizeof(InputNodeBufferPosition);
        send_sge.lkey = _mr_send->lkey;
        memset(&send_wr, 0, sizeof send_wr);
        send_wr.wr_id = ID_SEND_CN_WP;
        send_wr.opcode = IBV_WR_SEND;
        send_wr.sg_list = &send_sge;
        send_wr.num_sge = 1;

        // post initial receive request
        postRecvCnAck();
    }

private:

    /// Post a receive work request (WR) to the receive queue
    void postRecvCnAck() {
        if (Log.beTrace()) {
            Log.trace() << "[" << _index << "] "
                        << "POST RECEIVE _receive_cn_ack";
        }
        postRecv(&recv_wr);
    }

    /// Post a send work request (WR) to the send queue
    void postSendCnWp() {
        if (Log.beTrace()) {
            Log.trace() << "[" << _index << "] "
                        << "POST SEND _send_cp_wp (data=" << _send_cn_wp.data
                        << " desc=" << _send_cn_wp.desc << ")";
        }
        postSend(&send_wr);
    }

    /// Flag, true if it is the input nodes's turn to send a pointer update.
    bool _ourTurn;

    /// Local copy of acknowledged-by-CN pointers
    InputNodeBufferPosition _cn_ack;

    /// Receive buffer for acknowledged-by-CN pointers
    InputNodeBufferPosition _receive_cn_ack;

    /// Infiniband memory region descriptor for acknowledged-by-CN pointers
    struct ibv_mr* _mr_recv;

    /// Mutex protecting access to acknowledged-by-CN pointers
    boost::mutex _cn_ack_mutex;

    /// Condition variable for acknowledged-by-CN pointers    
    boost::condition_variable_any _cn_ack_cond;

    /// Local version of CN write pointers
    InputNodeBufferPosition _cn_wp;

    /// Send buffer for CN write pointers
    InputNodeBufferPosition _send_cn_wp;

    /// Infiniband memory region descriptor for CN write pointers
    struct ibv_mr* _mr_send;

    /// Mutex protecting access to CN write pointers
    boost::mutex _cn_wp_mutex;
   
    /// InfiniBand receive work request
    struct ibv_recv_wr recv_wr;

    /// Scatter/gather list entry for receive work request
    struct ibv_sge recv_sge;

    /// Infiniband send work request
    struct ibv_sge send_sge;

    /// Scatter/gather list entry for send work request
    struct ibv_send_wr send_wr;
};


#endif /* INPUTNODECONNECTION_HPP */
