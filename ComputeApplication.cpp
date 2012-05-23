/*
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

#include "Application.hpp"
#include "Timeslice.hpp"
#include "global.hpp"


ComputeNodeBufferPosition _send_cn_ack = {0};
ComputeNodeBufferPosition _cn_ack = {0};

ComputeNodeBufferPosition _recv_cn_wp = {0};
ComputeNodeBufferPosition _cn_wp = {0};

/// Access information for a remote memory region.
typedef struct {
    uint64_t addr; ///< Target memory address
    uint32_t rkey; ///< Target remote access key
} ServerInfo;

int
ComputeApplication::run()
{
    enum { ID_SEND = 4, ID_RECEIVE };

    Log.debug() << "Setting up RDMA CM structures";

    // Create an rdma event channel
    struct rdma_event_channel* cm_channel = rdma_create_event_channel();
    if (!cm_channel)
        throw ApplicationException("event channel creation failed");

    // Create rdma id (for listening)
    struct rdma_cm_id* listen_id;
    int err = rdma_create_id(cm_channel, &listen_id, NULL, RDMA_PS_TCP);
    if (err)
        throw ApplicationException("id creation failed");

    // Bind rdma id (for listening) to socket address (local port)
    unsigned short port = Par->basePort() + _par.nodeIndex();
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;
    err = rdma_bind_addr(listen_id, (struct sockaddr*) & sin);
    if (err)
        throw ApplicationException("RDMA bind_addr failed");

    // Listen for connection request on rdma id
    err = rdma_listen(listen_id, 1);
    if (err)
        throw ApplicationException("RDMA listen failed");

    Log.info() << "Waiting for connection";

    // Retrieve the next pending communication event (BLOCKING)
    struct rdma_cm_event* event;
    err = rdma_get_cm_event(cm_channel, &event);
    if (err)
        throw ApplicationException("retrieval of communication event failed");

    // Assert that event is a new connection request
    // Retrieve rdma id (for communicating) from event
    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST)
        throw ApplicationException("connection failed");
    struct rdma_cm_id* cm_id = event->id;

    // Free the communication event
    rdma_ack_cm_event(event);

    Log.debug() << "Creating verbs objects";

    // Allocate a protection domain (PD) for the given context
    struct ibv_pd* pd = ibv_alloc_pd(cm_id->verbs);
    if (!pd)
        throw ApplicationException("allocation of protection domain failed");

    // Create a completion event channel for the given context
    struct ibv_comp_channel* comp_chan = ibv_create_comp_channel(cm_id->verbs);
    if (!comp_chan)
        throw ApplicationException("creation of completion event channel failed");

    // Create a completion queue (CQ) for the given context with at least 20
    // entries, using the given completion channel to return completion events
    struct ibv_cq*  cq = ibv_create_cq(cm_id->verbs, 20, NULL, comp_chan, 0);
    if (!cq)
        throw ApplicationException("creation of completion queue failed");

    // Request a completion notification on the given completion queue
    // ("one shot" - only one completion event will be generated)
    if (ibv_req_notify_cq(cq, 0))
        throw ApplicationException("request of completion notification failed");

    // Allocate buffer space
    uint64_t* _data = (uint64_t*) calloc(Par->cnDataBufferSize(), sizeof(uint64_t));
    TimesliceComponentDescriptor* _desc = (TimesliceComponentDescriptor*) calloc(Par->cnDescBufferSize(), sizeof(TimesliceComponentDescriptor));
    if (!_data || !_desc)
        throw ApplicationException("allocation of buffer space failed");

    // Register a memory region (MR) associated with the given protection domain
    // (local read access is always enabled)
    struct ibv_mr* mr_data = ibv_reg_mr(pd, _data,
                                        Par->cnDataBufferSize() * sizeof(uint64_t),
                                        IBV_ACCESS_LOCAL_WRITE |
                                        IBV_ACCESS_REMOTE_WRITE);
    struct ibv_mr* mr_desc = ibv_reg_mr(pd, _desc,
                                        Par->cnDescBufferSize() * sizeof(TimesliceComponentDescriptor),
                                        IBV_ACCESS_LOCAL_WRITE |
                                        IBV_ACCESS_REMOTE_WRITE);
    struct ibv_mr* mr_send = ibv_reg_mr(pd, &_send_cn_ack,
                                        sizeof(ComputeNodeBufferPosition), 0);
    struct ibv_mr* mr_recv = ibv_reg_mr(pd, &_recv_cn_wp,
                                        sizeof(ComputeNodeBufferPosition),
                                        IBV_ACCESS_LOCAL_WRITE);
    if (!mr_data || !mr_desc || !mr_recv || !mr_send)
        throw ApplicationException("registration of memory region failed");

    // Allocate a queue pair (QP) associated with the specified rdma id
    struct ibv_qp_init_attr qp_attr;
    memset(&qp_attr, 0, sizeof qp_attr);
    qp_attr.cap.max_send_wr = 1;  // max num of outstanding WRs in the SQ
    qp_attr.cap.max_send_sge = 1; // max num of outstanding scatter/gather
    // elements in a WR in the SQ
    qp_attr.cap.max_recv_wr = 1;  // max num of outstanding WRs in the RQ
    qp_attr.cap.max_recv_sge = 1; // max num of outstanding scatter/gather
    // elements in a WR in the RQ
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC; // reliable connection
    err = rdma_create_qp(cm_id, pd, &qp_attr);
    if (err)
        throw ApplicationException("creation of QP failed");

    Log.debug() << "Post receive before accepting connection";

    // post initial receive request
    {
        struct ibv_sge sge;
        sge.addr = (uintptr_t) &_recv_cn_wp;
        sge.length = sizeof(ComputeNodeBufferPosition);
        sge.lkey = mr_recv->lkey;
        struct ibv_recv_wr recv_wr;
        memset(&recv_wr, 0, sizeof recv_wr);
        recv_wr.wr_id = ID_RECEIVE;
        recv_wr.sg_list = &sge;
        recv_wr.num_sge = 1;
        struct ibv_recv_wr* bad_recv_wr;
        if (ibv_post_recv(cm_id->qp, &recv_wr, &bad_recv_wr))
            throw ApplicationException("post_recv failed");
    }

    Log.debug() << "accepting connection";

    // Accept rdma connection request
    ServerInfo rep_pdata[2];
    rep_pdata[0].addr = (uintptr_t) _data;
    rep_pdata[0].rkey = mr_data->rkey;
    rep_pdata[1].addr = (uintptr_t) _desc;
    rep_pdata[1].rkey = mr_desc->rkey;
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof conn_param);
    conn_param.responder_resources = 1;
    conn_param.private_data = rep_pdata;
    conn_param.private_data_len = sizeof rep_pdata;
    err = rdma_accept(cm_id, &conn_param);
    if (err)
        throw ApplicationException("RDMA accept failed");

    // Retrieve next pending communication event on the given channel (BLOCKING)
    err = rdma_get_cm_event(cm_channel, &event);
    if (err)
        throw ApplicationException("retrieval of communication event failed");

    // Assert that a connection has been established with the remote end point
    if (event->event != RDMA_CM_EVENT_ESTABLISHED)
        throw ApplicationException("connection could not be established");

    // Free the communication event
    rdma_ack_cm_event(event);

    Log.info() << "connection established";

    while (1) {
        // Wait for the next completion event in the given channel (BLOCKING)
        struct ibv_cq* ev_cq;
        void* ev_ctx;

        if (ibv_get_cq_event(comp_chan, &ev_cq, &ev_ctx))
            throw ApplicationException("retrieval of cq event failed");

        // Acknowledge the completion queue (CQ) event
        ibv_ack_cq_events(ev_cq, 1);

        if (ev_cq != cq)
            throw ApplicationException("CQ event for unknown CQ");

        // Request a completion notification on the given completion queue
        // ("one shot" - only one completion event will be generated)
        if (ibv_req_notify_cq(cq, 0))
            throw ApplicationException("request of completion notification failed");
        struct ibv_wc wc[1];
        int ne;

        // Poll the completion queue (CQ) for work completions (WC)
        ne = ibv_poll_cq(cq, 1, wc);
        if (ne < 0)
            throw ApplicationException("polling the completion queue failed");

        for (int i = 0; i < ne; i++) {
            if (wc[i].status != IBV_WC_SUCCESS) {
                std::ostringstream s;
                s << "failed status " << ibv_wc_status_str(wc[i].status)
                  << " (" << wc[i].status << ") for wr_id "
                  << (int) wc[i].wr_id;
                throw ApplicationException(s.str());
            }

            switch ((int) wc[i].wr_id) {
            case ID_SEND:
                Log.debug() << "SEND complete";
                break;

            case ID_RECEIVE:
                // post new receive request
            {
                struct ibv_sge sge;
                sge.addr = (uintptr_t) &_recv_cn_wp;
                sge.length = sizeof(ComputeNodeBufferPosition);
                sge.lkey = mr_recv->lkey;
                struct ibv_recv_wr recv_wr;
                memset(&recv_wr, 0, sizeof recv_wr);
                recv_wr.wr_id = ID_RECEIVE;
                recv_wr.sg_list = &sge;
                recv_wr.num_sge = 1;
                struct ibv_recv_wr* bad_recv_wr;
                if (ibv_post_recv(cm_id->qp, &recv_wr, &bad_recv_wr))
                    throw ApplicationException("post_recv failed");
            }
            _cn_wp = _recv_cn_wp;
            // debug output
            Log.debug() << "RECEIVE _cn_wp: data=" << _cn_wp.data
                        << " desc=" << _cn_wp.desc;
#ifdef CHATTY
            std::cout << "/--- data buf ---" << std::endl << "|";
            for (unsigned int i = tscdesc.offset;
                    i < tscdesc.offset + tscdesc.size; i++) {
                std::cout << " (" << (i % Par->cnDataBufferSize()) << ")" << std::hex
                          << _data[i % Par->cnDataBufferSize()]
                          << std::dec;
            }
            std::cout << std::endl << "\\---------" << std::endl;
#endif
            // end debug output

            // check buffer contents
            //boost::this_thread::sleep(boost::posix_time::millisec(500));
            // end check buffer contents

            // DEBUG: empty the buffer
            _cn_ack = _cn_wp;

            // send ack
            {
                _send_cn_ack = _cn_ack;
                Log.debug() << "SEND posted";
                struct ibv_sge sge3;
                sge3.addr = (uintptr_t) &_send_cn_ack;
                sge3.length = sizeof(ComputeNodeBufferPosition);
                sge3.lkey = mr_send->lkey;
                struct ibv_send_wr send_wr2;
                memset(&send_wr2, 0, sizeof send_wr2);
                send_wr2.wr_id = ID_SEND;
                send_wr2.opcode = IBV_WR_SEND;
                send_wr2.send_flags = IBV_SEND_SIGNALED;
                send_wr2.sg_list = &sge3;
                send_wr2.num_sge = 1;
                struct ibv_send_wr* bad_send_wr;
                if (ibv_post_send(cm_id->qp, &send_wr2, &bad_send_wr))
                    throw ApplicationException("post_send failed");
            }
            break;

            default:
                throw ApplicationException("completion for unknown wr_id");
            }
        }
    }

    return 0;
}



