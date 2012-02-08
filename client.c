/*
 * usage:
 *   client <servername> <val1> <val2>
 *
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
#include "common.h"

enum {
    RESOLVE_TIMEOUT_MS = 5000,
};

struct pdata {
    uint64_t buf_va;
    uint32_t buf_rkey;
};

int 
run_client(int argc, char *argv[])
{
    DEBUG("Setting up RDMA CM structures");

    // Create an rdma event channel
    struct rdma_event_channel *cm_channel = rdma_create_event_channel();
    if (!cm_channel) {
	ERROR("event channel creation failed");
	return 1;
    }

    // Create rdma id (for listening)
    struct rdma_cm_id *cm_id;
    int err = rdma_create_id(cm_channel, &cm_id, NULL, RDMA_PS_TCP);
    if (err) {
	ERROR("id creation failed");
	return err;
    }
    
    // Retrieve a list of IP addresses and port numbers
    // for given hostname and service
    struct addrinfo hints = {
	.ai_family = AF_INET,
	.ai_socktype = SOCK_STREAM
    };
    struct addrinfo *res;
    err = getaddrinfo(argv[1], "20079", &hints, &res);
    if (err) {
	ERROR("getaddrinfo failed");
	return 1;
    }
  
    DEBUG("resolution of server address and route");

    // Resolve destination address from IP address to rdma address
    // (binds cm_id to a local device)
    for (struct addrinfo *t = res; t; t = t->ai_next) {
	err = rdma_resolve_addr(cm_id, NULL, t->ai_addr, RESOLVE_TIMEOUT_MS);
	if (!err)
	    break;
    }
    if (err) {
	ERROR("RDMA address resolution failed");
	return err;
    }

    // Retrieve the next pending communication event    
    struct rdma_cm_event *event;
    err = rdma_get_cm_event(cm_channel, &event);
    if (err) {
	ERROR("retrieval of communication event failed");
	return err;
    }

    // Assert that event is address resolution completion
    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
	ERROR("RDMA address resolution failed");
	return 1;
    }

    // Free the communication event
    rdma_ack_cm_event(event);

    // Resolv an rdma route to the dest address to establish a connection
    err = rdma_resolve_route(cm_id, RESOLVE_TIMEOUT_MS);
    if (err) {
	ERROR("RDMA route resolution failed");
	return err;
    }

    // Retrieve the next pending communication event
    err = rdma_get_cm_event(cm_channel, &event);
    if (err) {
	ERROR("retrieval of communication event failed");
	return err;
    }

    // Assert that event is route resolution completion
    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
	ERROR("RDMA route resolution failed");
	return 1;
    }

    // Free the communication event
    rdma_ack_cm_event(event);

    DEBUG("creating verbs objects");

    // Allocate a protection domain (PD) for the given context
    struct ibv_pd *pd = ibv_alloc_pd(cm_id->verbs);
    if (!pd) {
	ERROR("allocation of protection domain failed");
	return 1;
    }

    // Create a completion event channel for the given context
    struct ibv_comp_channel *comp_chan = ibv_create_comp_channel(cm_id->verbs);
    if (!comp_chan) {
	ERROR("creation of completion event channel failed");
	return 1;
    }

    // Create a completion queue (CQ) for the given context with at least 2
    // entries, using the given completion channel to return completion events
    struct ibv_cq *cq = ibv_create_cq(cm_id->verbs, 2, NULL, comp_chan, 0);
    if (!cq) {
	ERROR("creation of completion queue failed");
	return 1;
    }

    // Request a completion notification on the given completion queue
    // ("one shot" - only one completion event will be generated)
    if (ibv_req_notify_cq(cq, 0)) {
	ERROR("request of completion notification failed");
	return 1;
    }

    // Allocate buffer space
    uint32_t *buf = calloc(2, sizeof(uint32_t));
    if (!buf) {
	ERROR("allocation of buffer space failed");
	return 1;
    }
    
    // Register a memory region (MR) associated with the given protection domain
    // (local read access is always enabled)
    struct ibv_mr *mr = ibv_reg_mr(pd, buf, 2 * sizeof(uint32_t),
                                   IBV_ACCESS_LOCAL_WRITE);
    if (!mr) {
	ERROR("registration of memory region for RECV failed");
	return 1;
    }

    // Allocate a queue pair (QP) associated with the specified rdma id    
    struct ibv_qp_init_attr qp_attr = {
        .cap.max_send_wr = 2,  // max num of outstanding WRs in the SQ
	.cap.max_send_sge = 1, // max num of outstanding scatter/gather
                               // elements in a WR in the SQ
	.cap.max_recv_wr = 1,  // max num of outstanding WRs in the RQ
	.cap.max_recv_sge = 1, // max num of outstanding scatter/gather
                               // elements in a WR in the RQ
	.send_cq = cq,
	.recv_cq = cq,
	.qp_type = IBV_QPT_RC  // reliable connection
    };
    err = rdma_create_qp(cm_id, pd, &qp_attr);
    if (err) {
	ERROR("creation of QP failed");
	return err;
    }
    
    DEBUG("Connect to server");

    // Initiate an active connection request
    struct rdma_conn_param conn_param = {
        .initiator_depth = 1,
	.retry_count = 7
    };
    err = rdma_connect(cm_id, &conn_param);
    if (err) {
	ERROR("RDMA connect failed");
	return err;
    }

    // Retrieve next pending communication event on the given channel (BLOCKING)
    err = rdma_get_cm_event(cm_channel, &event);
    if (err) {
	ERROR("retrieval of communication event failed");
	return err;
    }

    // Assert that a connection has been established with the remote end point
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
	ERROR("connection could not be established");
	return 1;
    }

    // Copy server private data from event
    struct pdata server_pdata;
    memcpy(&server_pdata, event->param.conn.private_data,
	   sizeof server_pdata);

    // Free the communication event
    rdma_ack_cm_event(event);

    DEBUG("Prepost receive");

    // Post a receive work request (WR) to the receive queue
    // (received value will be written to buf[0])
    struct ibv_sge sge = {
        .addr = (uintptr_t) buf,
	.length = sizeof(uint32_t),
	.lkey = mr->lkey
    };
    struct ibv_recv_wr recv_wr = {
        .wr_id = 0,
        .sg_list = &sge,
        .num_sge = 1
    };
    struct ibv_recv_wr *bad_recv_wr;
    if (ibv_post_recv(cm_id->qp, &recv_wr, &bad_recv_wr)) {
	ERROR("post_recv failed");
	return 1;
    }

    DEBUG("Write/send two integers to be added");

    // Fill buf[] with 2 values from command line
    buf[0] = strtoul(argv[2], NULL, 0);
    buf[1] = strtoul(argv[3], NULL, 0);
    printf("%d + %d = \n", buf[0], buf[1]);
    buf[0] = htonl(buf[0]);
    buf[1] = htonl(buf[1]);

    // Post an rdma write work request (WR) to the send queue
    // (rdma write value of local buf[0] to remote buf[0])
    struct ibv_sge sge2 = {
        .addr = (uintptr_t) buf,
        .length = sizeof(uint32_t),
        .lkey = mr->lkey
    };
    struct ibv_send_wr send_wr = {
        .wr_id = 1,
        .opcode = IBV_WR_RDMA_WRITE,
        .sg_list = &sge2,
        .num_sge = 1,
        .wr.rdma.rkey = ntohl(server_pdata.buf_rkey),
        .wr.rdma.remote_addr = ntohll(server_pdata.buf_va),
    };
    struct ibv_send_wr *bad_send_wr;
    if (ibv_post_send(cm_id->qp, &send_wr, &bad_send_wr)) {
	ERROR("post_send failed");
	return 1;
    }

    // Post a send work request (WR) to the send queue
    // (send value of buf[1])
    struct ibv_sge sge3 = {
        .addr = (uintptr_t) buf + sizeof(uint32_t),
	.length = sizeof(uint32_t),
	.lkey = mr->lkey
    };
    struct ibv_send_wr send_wr2 = {
        .wr_id = 2,
        .opcode = IBV_WR_SEND,
        .send_flags = IBV_SEND_SIGNALED,
        .sg_list = &sge3,
        .num_sge = 1
    };
    if (ibv_post_send(cm_id->qp, &send_wr2, &bad_send_wr)) {
	ERROR("post_send failed");
	return 1;
    }
    
    DEBUG("Wait for receive completion");

    while (1) {
        // Wait for the next completion event in the given channel (BLOCKING)
        struct ibv_cq *evt_cq;
        void *cq_context;
	if (ibv_get_cq_event(comp_chan, &evt_cq, &cq_context)) {
	    ERROR("retrieval of cq event failed");
	    return 1;
	}

        // Request a completion notification on the given completion queue
        // ("one shot" - only one completion event will be generated)
	if (ibv_req_notify_cq(cq, 0)) {
	    ERROR("request of completion notification failed");
	    return 1;
	}
        
        struct ibv_wc wc;
        int n;
        // Poll the completion queue (CQ) for 1 work completion (WC)
	while ((n = ibv_poll_cq(cq, 1, &wc)) > 0) {
	    if (wc.status != IBV_WC_SUCCESS) {
		ERROR("SEND was unsuccessful");
		return 1;
	    }
	    if (wc.wr_id == 0) {
		DEBUG("transmission successful");
		printf("%d\n", ntohl(buf[0]));
		return 0;
	    }
	}
	if (n < 0) {
	    ERROR("polling the completion queue failed");
	    return 1;
	}
    }

    return 0;
}
