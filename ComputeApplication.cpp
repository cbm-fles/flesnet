/**
 * ComputeApplication.cpp
 *
 * 2012, Jan de Cuveland
 */

#include <cstdlib>
#include <cstring>
#include <sstream>

#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

#include <boost/thread.hpp>

#include "Infiniband.hpp"
#include "Application.hpp"
#include "Timeslice.hpp"
#include "global.hpp"


enum { ID_SEND = 4, ID_RECEIVE, ID_SEND_FINALIZE };


class ComputeNodeConnection : public IBConnection
{
public:
    ComputeNodeConnection(struct rdma_event_channel* ec, int index, struct rdma_cm_id* id = 0) :
        IBConnection(ec, index, id),
        _data(0),
        _desc(0),
        _mr_data(0),
        _mr_desc(0),
        _mr_send(0),
        _mr_recv(0)
    {
        memset(&_send_cn_ack, 0, sizeof(ComputeNodeBufferPosition));
        memset(&_cn_ack, 0, sizeof(ComputeNodeBufferPosition));
        memset(&_cn_wp, 0, sizeof(ComputeNodeBufferPosition));
        memset(&_recv_cn_wp, 0, sizeof(ComputeNodeBufferPosition));

        // Allocate buffer space
        _data = (uint64_t*) calloc(Par->cnDataBufferSize(), sizeof(uint64_t));
        _desc = (TimesliceComponentDescriptor*) calloc(Par->cnDescBufferSize(),
                                                       sizeof(TimesliceComponentDescriptor));
        if (!_data || !_desc)
            throw ApplicationException("allocation of buffer space failed");
    }

    void post_receive() {
        struct ibv_sge sge;
        sge.addr = (uintptr_t) &_recv_cn_wp;
        sge.length = sizeof(ComputeNodeBufferPosition);
        sge.lkey = _mr_recv->lkey;
        struct ibv_recv_wr recv_wr;
        memset(&recv_wr, 0, sizeof recv_wr);
        recv_wr.wr_id = ID_RECEIVE;
        recv_wr.sg_list = &sge;
        recv_wr.num_sge = 1;
        postRecv(&recv_wr);
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
        send_wr.wr_id = final ? ID_SEND_FINALIZE : ID_SEND;
        send_wr.opcode = IBV_WR_SEND;
        send_wr.send_flags = IBV_SEND_SIGNALED;
        send_wr.sg_list = &sge;
        send_wr.num_sge = 1;
        postSend(&send_wr);
    }

    virtual void onConnectRequest(struct ibv_pd* pd, struct ibv_cq* cq) {
        IBConnection::onConnectRequest(pd, cq);
        
        // register memory regions
        _mr_data = ibv_reg_mr(pd, _data,
                              Par->cnDataBufferSize() * sizeof(uint64_t),
                              IBV_ACCESS_LOCAL_WRITE |
                              IBV_ACCESS_REMOTE_WRITE);
        _mr_desc = ibv_reg_mr(pd, _desc,
                              Par->cnDescBufferSize() * sizeof(TimesliceComponentDescriptor),
                              IBV_ACCESS_LOCAL_WRITE |
                              IBV_ACCESS_REMOTE_WRITE);
        _mr_send = ibv_reg_mr(pd, &_send_cn_ack,
                              sizeof(ComputeNodeBufferPosition), 0);
        _mr_recv = ibv_reg_mr(pd, &_recv_cn_wp,
                              sizeof(ComputeNodeBufferPosition),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_data || !_mr_desc || !_mr_recv || !_mr_send)
            throw ApplicationException("registration of memory region failed");

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
        int err = rdma_accept(_cmId, &conn_param);
        if (err)
            throw ApplicationException("RDMA accept failed");
    }

    void
    checkBuffer(ComputeNodeBufferPosition ack, ComputeNodeBufferPosition wp,
                TimesliceComponentDescriptor* desc, void *data)
    {
        int ts = wp.desc - ack.desc;
        Log.debug() << "received " << ts << " timeslices";
        for (uint64_t dp = ack.desc; dp < wp.desc; dp++) {
            TimesliceComponentDescriptor tcd = desc[dp % Par->cnDescBufferSize()];
            Log.debug() << "checking ts #" << tcd.tsNum;
        }
    }

    void onCompleteRecv()
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
        checkBuffer(_cn_ack, _cn_wp, _desc, _data);

        // DEBUG: empty the buffer
        _cn_ack = _cn_wp;

        // send ack
        send_ack();
    }

    ComputeNodeBufferPosition _send_cn_ack;
    ComputeNodeBufferPosition _cn_ack;

    ComputeNodeBufferPosition _recv_cn_wp;
    ComputeNodeBufferPosition _cn_wp;

    uint64_t* _data;
    TimesliceComponentDescriptor* _desc;

    struct ibv_mr* _mr_data;
    struct ibv_mr* _mr_desc;
    struct ibv_mr* _mr_send;
    struct ibv_mr* _mr_recv;

};


class ComputeBuffer : public IBConnectionGroup<ComputeNodeConnection>
{
public:
    /// Completion notification event dispatcher. Called by the event loop.
    virtual void onCompletion(const struct ibv_wc& wc) {
        switch (wc.wr_id & 0xFF) {
        case ID_SEND:
            Log.debug() << "SEND complete";
            break;

        case ID_SEND_FINALIZE:
            Log.debug() << "SEND FINALIZE complete";
            _allDone = true;
            break;

        case ID_RECEIVE:
            _conn[0]->onCompleteRecv();            
            break;

        default:
            throw InfinibandException("wc for unknown wr_id");
        }
    }
};


int
ComputeApplication::run()
{
    ComputeBuffer* cb = new ComputeBuffer();
    cb->accept(Par->basePort() + Par->nodeIndex());
    cb->handleCmEvents(true);
    
    /// DEBUG v
    boost::thread t1(&ComputeBuffer::handleCmEvents, cb, false);
    /// DEBUG ^

    cb->completionHandler();
    delete cb;
    
    return 0;
}



