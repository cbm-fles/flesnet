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


struct rdma_event_channel* cm_channel;

/// The connection manager event loop.
void debugHandleCmEvents() {
    int err;
    struct rdma_cm_event* event;
    
    while ((err = rdma_get_cm_event(cm_channel, &event)) == 0) {
        Log.info() << "DELME GOT cm event";
        //int err = onCmEvent(event);

        struct rdma_cm_id* cm_id = event->id;
        switch (event->event) {
        case RDMA_CM_EVENT_ADDR_RESOLVED:
            throw InfinibandException("gaga");
        case RDMA_CM_EVENT_ADDR_ERROR:
            throw InfinibandException("rdma_resolve_addr failed");
        case RDMA_CM_EVENT_ROUTE_RESOLVED:
            throw InfinibandException("gaga");
        case RDMA_CM_EVENT_ROUTE_ERROR:
            throw InfinibandException("rdma_resolve_route failed");
        case RDMA_CM_EVENT_CONNECT_ERROR:
            throw InfinibandException("could not establish connection");
        case RDMA_CM_EVENT_UNREACHABLE:
            throw InfinibandException("remote server is not reachable");
        case RDMA_CM_EVENT_REJECTED:
            throw InfinibandException("request rejected by remote endpoint");
        case RDMA_CM_EVENT_ESTABLISHED:
            throw InfinibandException("gaga");
        case RDMA_CM_EVENT_DISCONNECTED:
            Log.info() << "DELME disconnect event";
            rdma_disconnect(cm_id);
            break;
        case RDMA_CM_EVENT_CONNECT_REQUEST:
        case RDMA_CM_EVENT_CONNECT_RESPONSE:
        case RDMA_CM_EVENT_DEVICE_REMOVAL:
        case RDMA_CM_EVENT_MULTICAST_JOIN:
        case RDMA_CM_EVENT_MULTICAST_ERROR:
        case RDMA_CM_EVENT_ADDR_CHANGE:
            throw InfinibandException("unspecified cm event");
        case RDMA_CM_EVENT_TIMEWAIT_EXIT:
            Log.info() << "RDMA_CM_EVENT_TIMEWAIT_EXIT";
            break;
        default:
            throw InfinibandException("unknown cm event");
        }

        
        rdma_ack_cm_event(event);
        if (err)
            break;
    };
    if (err)
        throw InfinibandException("rdma_get_cm_event failed");
}


enum { ID_SEND = 4, ID_RECEIVE, ID_SEND_FINALIZE };

struct connparams {
    uint64_t* _data;
    TimesliceComponentDescriptor* _desc;
    struct ibv_mr* _mr_data;
    struct ibv_mr* _mr_desc;
    struct ibv_mr* _mr_send;
    struct ibv_mr* _mr_recv;
    struct rdma_cm_id* _cm_id;
    struct ibv_comp_channel* _comp_chan;
    struct ibv_cq* _cq;
} the_cp;


void post_receive() {
    struct ibv_sge sge;
    sge.addr = (uintptr_t) &_recv_cn_wp;
    sge.length = sizeof(ComputeNodeBufferPosition);
    sge.lkey = the_cp._mr_recv->lkey;
    struct ibv_recv_wr recv_wr;
    memset(&recv_wr, 0, sizeof recv_wr);
    recv_wr.wr_id = ID_RECEIVE;
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;
    struct ibv_recv_wr* bad_recv_wr;
    if (ibv_post_recv(the_cp._cm_id->qp, &recv_wr, &bad_recv_wr))
        throw ApplicationException("post_recv failed");
}


void connect()
{
    Log.debug() << "Setting up RDMA CM structures";

    // Create an rdma event channel
    cm_channel = rdma_create_event_channel();
    if (!cm_channel)
        throw ApplicationException("event channel creation failed");

    // Create rdma id (for listening)
    struct rdma_cm_id* listen_id;
    int err = rdma_create_id(cm_channel, &listen_id, NULL, RDMA_PS_TCP);
    if (err)
        throw ApplicationException("id creation failed");

    // Bind rdma id (for listening) to socket address (local port)
    unsigned short port = Par->basePort() + Par->nodeIndex();
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
    the_cp._cm_id = event->id;

    // Free the communication event
    rdma_ack_cm_event(event);

    Log.debug() << "Creating verbs objects";

    // Allocate a protection domain (PD) for the given context
    struct ibv_pd* pd = ibv_alloc_pd(the_cp._cm_id->verbs);
    if (!pd)
        throw ApplicationException("allocation of protection domain failed");

    // Create a completion event channel for the given context
    the_cp._comp_chan = ibv_create_comp_channel(the_cp._cm_id->verbs);
    if (!the_cp._comp_chan)
        throw ApplicationException("creation of completion event channel failed");

    // Create a completion queue (CQ) for the given context with at least 20
    // entries, using the given completion channel to return completion events
    the_cp._cq = ibv_create_cq(the_cp._cm_id->verbs, 20, NULL, the_cp._comp_chan, 0);
    if (!the_cp._cq)
        throw ApplicationException("creation of completion queue failed");

    // Request a completion notification on the given completion queue
    // ("one shot" - only one completion event will be generated)
    if (ibv_req_notify_cq(the_cp._cq, 0))
        throw ApplicationException("request of completion notification failed");

    // Allocate buffer space
    the_cp._data = (uint64_t*) calloc(Par->cnDataBufferSize(), sizeof(uint64_t));
    the_cp._desc = (TimesliceComponentDescriptor*) calloc(Par->cnDescBufferSize(), sizeof(TimesliceComponentDescriptor));
    if (!the_cp._data || !the_cp._desc)
        throw ApplicationException("allocation of buffer space failed");

    // Register a memory region (MR) associated with the given protection domain
    // (local read access is always enabled)
    the_cp._mr_data = ibv_reg_mr(pd, the_cp._data,
                                 Par->cnDataBufferSize() * sizeof(uint64_t),
                                 IBV_ACCESS_LOCAL_WRITE |
                                 IBV_ACCESS_REMOTE_WRITE);
    the_cp._mr_desc = ibv_reg_mr(pd, the_cp._desc,
                                 Par->cnDescBufferSize() * sizeof(TimesliceComponentDescriptor),
                                 IBV_ACCESS_LOCAL_WRITE |
                                 IBV_ACCESS_REMOTE_WRITE);
    the_cp._mr_send = ibv_reg_mr(pd, &_send_cn_ack,
                                 sizeof(ComputeNodeBufferPosition), 0);
    the_cp._mr_recv = ibv_reg_mr(pd, &_recv_cn_wp,
                                 sizeof(ComputeNodeBufferPosition),
                                 IBV_ACCESS_LOCAL_WRITE);
    if (!the_cp._mr_data || !the_cp._mr_desc || !the_cp._mr_recv || !the_cp._mr_send)
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
    qp_attr.send_cq = the_cp._cq;
    qp_attr.recv_cq = the_cp._cq;
    qp_attr.qp_type = IBV_QPT_RC; // reliable connection
    err = rdma_create_qp(the_cp._cm_id, pd, &qp_attr);
    if (err)
        throw ApplicationException("creation of QP failed");

    Log.debug() << "Post receive before accepting connection";

    // post initial receive request
    post_receive();

    Log.debug() << "accepting connection";

    // Accept rdma connection request
    ServerInfo rep_pdata[2];
    rep_pdata[0].addr = (uintptr_t) the_cp._data;
    rep_pdata[0].rkey = the_cp._mr_data->rkey;
    rep_pdata[1].addr = (uintptr_t) the_cp._desc;
    rep_pdata[1].rkey = the_cp._mr_desc->rkey;
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof conn_param);
    conn_param.responder_resources = 1;
    conn_param.private_data = rep_pdata;
    conn_param.private_data_len = sizeof rep_pdata;
    err = rdma_accept(the_cp._cm_id, &conn_param);
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

}


bool _allDone = false;


void send_ack(bool final = false) {
    _send_cn_ack = final ? _recv_cn_wp : _cn_ack;
    Log.debug() << "SEND posted";
    struct ibv_sge sge;
    sge.addr = (uintptr_t) &_send_cn_ack;
    sge.length = sizeof(ComputeNodeBufferPosition);
    sge.lkey = the_cp._mr_send->lkey;
    struct ibv_send_wr send_wr;
    memset(&send_wr, 0, sizeof send_wr);
    send_wr.wr_id = final ? ID_SEND_FINALIZE : ID_SEND;
    send_wr.opcode = IBV_WR_SEND;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;
    struct ibv_send_wr* bad_send_wr;
    if (ibv_post_send(the_cp._cm_id->qp, &send_wr, &bad_send_wr))
        throw ApplicationException("post_send failed");
}


void onCompletion(const struct ibv_wc& wc) {
    switch (wc.wr_id & 0xFF) {
    case ID_SEND:
        Log.debug() << "SEND complete";
        break;

    case ID_SEND_FINALIZE:
        Log.debug() << "SEND FINALIZE complete";
        _allDone = true;
        break;

    case ID_RECEIVE:
        if (_recv_cn_wp.data == UINT64_MAX && _recv_cn_wp.desc == UINT64_MAX) {
            Log.info() << "received FINAL pointer update";
            // send FINAL ack
            send_ack(true);
            break;
        }
                
        // post new receive request
        post_receive();
        _cn_wp = _recv_cn_wp;
        // debug output
        Log.debug() << "RECEIVE _cn_wp: data=" << _cn_wp.data
                    << " desc=" << _cn_wp.desc;

        // check buffer contents
        checkBuffer(_cn_ack, _cn_wp, the_cp._desc, the_cp._data);

        // DEBUG: empty the buffer
        _cn_ack = _cn_wp;

        // send ack
        send_ack();
        break;

    default:
        throw InfinibandException("wc for unknown wr_id");
    }
}


/// The InfiniBand completion notification event loop.
void completionHandler() {
    const int ne_max = 10;
    
    struct ibv_cq* ev_cq;
    void* ev_ctx;
    struct ibv_wc wc[ne_max];
    int ne;
    
    while (!_allDone) {
        if (ibv_get_cq_event(the_cp._comp_chan, &ev_cq, &ev_ctx))
            throw InfinibandException("ibv_get_cq_event failed");
        
        ibv_ack_cq_events(ev_cq, 1);
        
        if (ev_cq != the_cp._cq)
            throw InfinibandException("CQ event for unknown CQ");
        
        if (ibv_req_notify_cq(the_cp._cq, 0))
            throw InfinibandException("ibv_req_notify_cq failed");
        
        while ((ne = ibv_poll_cq(the_cp._cq, ne_max, wc))) {
            if (ne < 0)
                throw InfinibandException("ibv_poll_cq failed");

            for (int i = 0; i < ne; i++) {
                if (wc[i].status != IBV_WC_SUCCESS) {
                    std::ostringstream s;
                    s << ibv_wc_status_str(wc[i].status)
                      << " for wr_id " << (int) wc[i].wr_id;
                    Log.error() << s.str();
                    continue;
                }
                
                onCompletion(wc[i]);
            }
        }
    }

    Log.info() << "COMPLETION loop done";
}


int
ComputeApplication::run()
{
    connect();
    /// DEBUG v
    boost::thread t1(&debugHandleCmEvents);
    /// DEBUG ^

    completionHandler();

    return 0;
}



