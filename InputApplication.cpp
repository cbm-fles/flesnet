/*
 * InputApplication.hpp
 *
 * 2012, Jan de Cuveland
 */

/*
 * connects to server, sends val1 via RDMA write and val2 via send,
 * and receives val1+val2 back from the server.
 *
 * Based on an example by Roland Dreier, http://www.digitalvampire.org/
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

#include <sys/time.h>

#include "Application.hpp"
#include "Parameters.hpp"


struct pdata {
    uint64_t buf_va;
    uint32_t buf_rkey;
};

int 
InputApplication::run()
{
    uint32_t int1 = 2;
    uint32_t int2 = 3;
    const char *hostname = _par.compute_nodes().at(0).c_str();
    
    DEBUG("Setting up RDMA CM structures");

    // Create an rdma event channel
    struct rdma_event_channel *cm_channel = rdma_create_event_channel();
    if (!cm_channel)
	throw ApplicationException("event channel creation failed");
    
    // Create rdma id (for listening)
    struct rdma_cm_id *cm_id;
    int err = rdma_create_id(cm_channel, &cm_id, NULL, RDMA_PS_TCP);
    if (err)
	throw ApplicationException("id creation failed");
    
    // Retrieve a list of IP addresses and port numbers
    // for given hostname and service
    struct addrinfo hints; 
    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_STREAM; 
    struct addrinfo *res;

    err = getaddrinfo(hostname, "20079", &hints, &res);
    if (err)
	throw ApplicationException("getaddrinfo failed");
  
    DEBUG("resolution of server address and route");

    // Resolve destination address from IP address to rdma address
    // (binds cm_id to a local device)
    for (struct addrinfo *t = res; t; t = t->ai_next) {
	err = rdma_resolve_addr(cm_id, NULL, t->ai_addr, RESOLVE_TIMEOUT_MS);
	if (!err)
	    break;
    }
    if (err)
        throw ApplicationException("RDMA address resolution failed");

    // Retrieve the next pending communication event    
    struct rdma_cm_event *event;
    err = rdma_get_cm_event(cm_channel, &event);
    if (err)
        throw ApplicationException("retrieval of communication event failed");

    // Assert that event is address resolution completion
    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED)
	throw ApplicationException("RDMA address resolution failed");

    // Free the communication event
    rdma_ack_cm_event(event);

    // Resolv an rdma route to the dest address to establish a connection
    err = rdma_resolve_route(cm_id, RESOLVE_TIMEOUT_MS);
    if (err)
	throw ApplicationException("RDMA route resolution failed");

    // Retrieve the next pending communication event
    err = rdma_get_cm_event(cm_channel, &event);
    if (err)
	throw ApplicationException("retrieval of communication event failed");

    // Assert that event is route resolution completion
    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED)
	throw ApplicationException("RDMA route resolution failed");

    // Free the communication event
    rdma_ack_cm_event(event);

    DEBUG("creating verbs objects");

    // Allocate a protection domain (PD) for the given context
    struct ibv_pd *pd = ibv_alloc_pd(cm_id->verbs);
    if (!pd)
	throw ApplicationException("allocation of protection domain failed");

    // Create a completion event channel for the given context
    struct ibv_comp_channel *comp_chan = ibv_create_comp_channel(cm_id->verbs);
    if (!comp_chan)
	throw ApplicationException("creation of completion event channel failed");

    // Create a completion queue (CQ) for the given context with at least 2
    // entries, using the given completion channel to return completion events
    struct ibv_cq *cq = ibv_create_cq(cm_id->verbs, 2, NULL, comp_chan, 0);
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
    struct ibv_mr *mr = ibv_reg_mr(pd, buf, 2 * sizeof(uint32_t),
                                   IBV_ACCESS_LOCAL_WRITE);
    if (!mr)
	throw ApplicationException("registration of memory region for RECV failed");

    // Allocate a queue pair (QP) associated with the specified rdma id    
    struct ibv_qp_init_attr qp_attr;
    memset(&qp_attr, 0, sizeof qp_attr); 
    qp_attr.cap.max_send_wr = 2;  // max num of outstanding WRs in the SQ
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
    
    DEBUG("Connect to server");

    // Initiate an active connection request
    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof conn_param); 
    conn_param.initiator_depth = 1;
    conn_param.retry_count = 7;
    err = rdma_connect(cm_id, &conn_param);
    if (err)
	throw ApplicationException("RDMA connect failed");

    // Retrieve next pending communication event on the given channel (BLOCKING)
    err = rdma_get_cm_event(cm_channel, &event);
    if (err)
	throw ApplicationException("retrieval of communication event failed");

    // Assert that a connection has been established with the remote end point
    if (event->event != RDMA_CM_EVENT_ESTABLISHED)
	throw ApplicationException("connection could not be established");

    // Copy server private data from event
    struct pdata server_pdata;
    memcpy(&server_pdata, event->param.conn.private_data,
	   sizeof server_pdata);

    // Free the communication event
    rdma_ack_cm_event(event);

    DEBUG("Prepost receive");

    // Post a receive work request (WR) to the receive queue
    // (received value will be written to buf[0])
    struct ibv_sge sge;
    memset(&sge, 0, sizeof sge); 
    sge.addr = (uintptr_t) buf;
    sge.length = sizeof(uint32_t);
    sge.lkey = mr->lkey;
    struct ibv_recv_wr recv_wr;
    memset(&recv_wr, 0, sizeof recv_wr); 
    recv_wr.wr_id = 0;
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;
    struct ibv_recv_wr *bad_recv_wr;
    if (ibv_post_recv(cm_id->qp, &recv_wr, &bad_recv_wr))
	throw ApplicationException("post_recv failed");

    DEBUG("Write/send two integers to be added");

    // Fill buf[] with 2 values
    buf[0] = int1;
    buf[1] = int2;
    printf("%d + %d = \n", buf[0], buf[1]);
    buf[0] = htonl(buf[0]);
    buf[1] = htonl(buf[1]);

    // Post an rdma write work request (WR) to the send queue
    // (rdma write value of local buf[0] to remote buf[0])
    struct ibv_sge sge2;
    memset(&sge2, 0, sizeof sge2); 
    sge2.addr = (uintptr_t) buf;
    sge2.length = sizeof(uint32_t);
    sge2.lkey = mr->lkey;
    struct ibv_send_wr send_wr;
    memset(&send_wr, 0, sizeof send_wr); 
    send_wr.wr_id = 1;
    send_wr.opcode = IBV_WR_RDMA_WRITE;
    send_wr.sg_list = &sge2;
    send_wr.num_sge = 1;
    send_wr.wr.rdma.rkey = ntohl(server_pdata.buf_rkey);
    send_wr.wr.rdma.remote_addr = ntohll(server_pdata.buf_va);
    struct ibv_send_wr *bad_send_wr;
    if (ibv_post_send(cm_id->qp, &send_wr, &bad_send_wr))
	throw ApplicationException("post_send failed");

    // Post a send work request (WR) to the send queue
    // (send value of buf[1])
    struct ibv_sge sge3;
    memset(&sge3, 0, sizeof sge3); 
    sge3.addr = (uintptr_t) buf + sizeof(uint32_t);
    sge3.length = sizeof(uint32_t);
    sge3.lkey = mr->lkey;
    struct ibv_send_wr send_wr2;
    memset(&send_wr2, 0, sizeof send_wr2); 
    send_wr2.wr_id = 2;
    send_wr2.opcode = IBV_WR_SEND;
    send_wr2.send_flags = IBV_SEND_SIGNALED;
    send_wr2.sg_list = &sge3;
    send_wr2.num_sge = 1;
    if (ibv_post_send(cm_id->qp, &send_wr2, &bad_send_wr))
	throw ApplicationException("post_send failed");
    
    DEBUG("Wait for receive completion");

    while (1) {
        // Wait for the next completion event in the given channel (BLOCKING)
        struct ibv_cq *evt_cq;
        void *cq_context;
	if (ibv_get_cq_event(comp_chan, &evt_cq, &cq_context))
	    throw ApplicationException("retrieval of cq event failed");

        // Request a completion notification on the given completion queue
        // ("one shot" - only one completion event will be generated)
	if (ibv_req_notify_cq(cq, 0))
	    throw ApplicationException("request of completion notification failed");
        
        struct ibv_wc wc;
        int n;
        // Poll the completion queue (CQ) for 1 work completion (WC)
	while ((n = ibv_poll_cq(cq, 1, &wc)) > 0) {
	    if (wc.status != IBV_WC_SUCCESS)
		throw ApplicationException("SEND was unsuccessful");
	    if (wc.wr_id == 0) {
		DEBUG("transmission successful");
		printf("%d\n", ntohl(buf[0]));
		return 0;
	    }
	}
	if (n < 0)
	    throw ApplicationException("polling the completion queue failed");
    }

    return 0;
}
