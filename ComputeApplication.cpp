/*
 * ComputeApplication.hpp
 *
 * 2012, Jan de Cuveland
 */

/*
 * waits for client to connect, receives two integers, and sends their
 * sum back to the client.
 *
 * Based on an example by Roland Dreier, http://www.digitalvampire.org/
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>

#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

#include "Application.hpp"

struct pdata {
    uint64_t buf_va;
    uint32_t buf_rkey;
};

int 
ComputeApplication::run()
{
    DEBUG("Setting up RDMA CM structures");

    // Create an rdma event channel
    struct rdma_event_channel *cm_channel = rdma_create_event_channel();
    if (!cm_channel)
        throw ApplicationException("event channel creation failed");

    // Create rdma id (for listening)
    struct rdma_cm_id *listen_id;
    int err = rdma_create_id(cm_channel, &listen_id, NULL, RDMA_PS_TCP);
    if (err)
        throw ApplicationException("id creation failed");

    // Bind rdma id (for listening) to socket address (local port)
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(20079);
    sin.sin_addr.s_addr = INADDR_ANY;
    err = rdma_bind_addr(listen_id, (struct sockaddr *) & sin);
    if (err)
        throw ApplicationException("RDMA bind_addr failed");

    // Listen for connection request on rdma id
    err = rdma_listen(listen_id, 1);
    if (err)
        throw ApplicationException("RDMA listen failed");

    DEBUG("Waiting for connection");

    // Retrieve the next pending communication event (BLOCKING)
    struct rdma_cm_event *event;
    err = rdma_get_cm_event(cm_channel, &event);
    if (err)
        throw ApplicationException("retrieval of communication event failed");

    // Assert that event is a new connection request
    // Retrieve rdma id (for communicating) from event
    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST)
        throw ApplicationException("connection failed");
    struct rdma_cm_id *cm_id = event->id;

    // Free the communication event
    rdma_ack_cm_event(event);

    DEBUG("Creating verbs objects");

    // Allocate a protection domain (PD) for the given context
    struct ibv_pd  *pd = ibv_alloc_pd(cm_id->verbs);
    if (!pd)
        throw ApplicationException("allocation of protection domain failed");

    // Create a completion event channel for the given context
    struct ibv_comp_channel *comp_chan = ibv_create_comp_channel(cm_id->verbs);
    if (!comp_chan)
        throw ApplicationException("creation of completion event channel failed");

    // Create a completion queue (CQ) for the given context with at least 2
    // entries, using the given completion channel to return completion events
    struct ibv_cq  *cq = ibv_create_cq(cm_id->verbs, 2, NULL, comp_chan, 0);
    if (!cq)
        throw ApplicationException("creation of completion queue failed");

    // Request a completion notification on the given completion queue
    // ("one shot" - only one completion event will be generated)
    if (ibv_req_notify_cq(cq, 0))
        throw ApplicationException("request of completion notification failed");

    // Allocate buffer space
    uint32_t* buf = (uint32_t *) calloc(2, sizeof(uint32_t));
    if (!buf)
        throw ApplicationException("allocation of buffer space failed");

    // Register a memory region (MR) associated with the given protection domain
    // (local read access is always enabled)
    struct ibv_mr  *mr = ibv_reg_mr(pd, buf, 2 * sizeof(uint32_t),
                                    IBV_ACCESS_LOCAL_WRITE |
                                    IBV_ACCESS_REMOTE_READ |
                                    IBV_ACCESS_REMOTE_WRITE);
    if (!mr)
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
  
    DEBUG("Post receive before accepting connection");

    // Post a receive work request (WR) to the receive queue
    // (received value will be written to buf[1])
    struct ibv_sge sge;
    memset(&sge, 0, sizeof sge);
    sge.addr = (uintptr_t) buf + sizeof(uint32_t);
    sge.length = sizeof(uint32_t);
    sge.lkey = mr->lkey;
    struct ibv_recv_wr recv_wr;
    memset(&recv_wr, 0, sizeof recv_wr);
    recv_wr.next = NULL;
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;
    struct ibv_recv_wr *bad_recv_wr;
    if (ibv_post_recv(cm_id->qp, &recv_wr, &bad_recv_wr))
        throw ApplicationException("post_recv failed");
  
    DEBUG("Accepting connection");

    // Accept rdma connection request
    struct pdata rep_pdata;
    memset(&rep_pdata, 0, sizeof rep_pdata);
    rep_pdata.buf_va = htonll((uintptr_t) buf);
    rep_pdata.buf_rkey = htonl(mr->rkey);
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof conn_param);
    conn_param.responder_resources = 1;
    conn_param.private_data = &rep_pdata;
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
  
    DEBUG("Wait for completion");

    // Wait for the next completion event in the given channel (BLOCKING)
    struct ibv_cq *evt_cq;
    void *cq_context;
    if (ibv_get_cq_event(comp_chan, &evt_cq, &cq_context))
        throw ApplicationException("retrieval of cq event failed");

    // Request a completion notification on the given completion queue
    // ("one shot" - only one completion event will be generated)
    if (ibv_req_notify_cq(cq, 0))
        throw ApplicationException("request of completion notification failed");

    // Poll the completion queue (CQ) for 1 work completion (WC)
    struct ibv_wc wc;
    if (ibv_poll_cq(cq, 1, &wc) < 1)
        throw ApplicationException("polling the completion queue failed");
    if (wc.opcode & IBV_WC_RECV)
        DEBUG("RECV completion received");
    if (wc.status != IBV_WC_SUCCESS)
        throw ApplicationException("transmission unsuccessful");
  
    DEBUG("Add two integers and send reply back");

    // Add the two numbers
    buf[0] = htonl(ntohl(buf[0]) + ntohl(buf[1]));

    // Post a send work request (WR) to the send queue
    struct ibv_sge sge2;
    memset(&sge2, 0, sizeof sge2);
    sge2.addr = (uintptr_t) buf;
    sge2.length = sizeof(uint32_t);
    sge2.lkey = mr->lkey;
    struct ibv_send_wr send_wr;
    memset(&send_wr, 0, sizeof send_wr);
    send_wr.opcode = IBV_WR_SEND;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.sg_list = &sge2;
    send_wr.num_sge = 1;
    struct ibv_send_wr *bad_send_wr;
    if (ibv_post_send(cm_id->qp, &send_wr, &bad_send_wr))
        throw ApplicationException("post_send failed");

    DEBUG("Wait for send completion");

    // Wait for the next completion event in the given channel
    if (ibv_get_cq_event(comp_chan, &evt_cq, &cq_context))
        throw ApplicationException("retrieval of cq event failed");

    // Poll the completion queue (CQ) for 1 work completion (WC)
    if (ibv_poll_cq(cq, 1, &wc) < 1)
        throw ApplicationException("polling the completion queue failed");
    if (wc.status != IBV_WC_SUCCESS)
        throw ApplicationException("SEND unsuccessful");
    DEBUG("SEND successful");

    // Acknowledge 2 completion queue (CQ) events
    ibv_ack_cq_events(cq, 2);

    return 0;
}
