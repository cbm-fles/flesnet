/**
 * \file IBConnection.cpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "IBConnection.hpp"
#include "Timeslice.hpp"
#include "global.hpp"
#include <netdb.h>
#include <cstring>
#include <cassert>

IBConnection::IBConnection(struct rdma_event_channel* ec,
                           uint_fast16_t connection_index,
                           uint_fast16_t remote_connection_index,
                           struct rdma_cm_id* id)
    : _index(connection_index),
      _remote_index(remote_connection_index),
      _cm_id(id)
{
    if (!_cm_id) {
        int err = rdma_create_id(ec, &_cm_id, this, RDMA_PS_TCP);
        if (err)
            throw InfinibandException("rdma_create_id failed");
    } else {
        _cm_id->context = this;
    }

    _qp_cap.max_send_wr = 16;
    _qp_cap.max_recv_wr = 16;
    _qp_cap.max_send_sge = 8;
    _qp_cap.max_recv_sge = 8;
    _qp_cap.max_inline_data = 0;
}

IBConnection::~IBConnection()
{
    if (_cm_id) {
        int err = rdma_destroy_id(_cm_id);
        if (err) {
            out.error() << "rdma_destroy_id() failed";
        }
        _cm_id = nullptr;
    }
}

void IBConnection::connect(const std::string& hostname,
                           const std::string& service)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res;

    int err = getaddrinfo(hostname.c_str(), service.c_str(), &hints, &res);
    if (err)
        throw InfinibandException("getaddrinfo failed");

    out.debug() << "[" << _index << "] "
                << "resolution of server address and route";

    for (struct addrinfo* t = res; t; t = t->ai_next) {
        err = rdma_resolve_addr(_cm_id, nullptr, t->ai_addr,
                                RESOLVE_TIMEOUT_MS);
        if (!err)
            break;
    }
    if (err)
        throw InfinibandException("rdma_resolve_addr failed");

    freeaddrinfo(res);
}

void IBConnection::disconnect()
{
    out.debug() << "[" << _index << "] "
                << "disconnect";
    int err = rdma_disconnect(_cm_id);
    if (err)
        throw InfinibandException("rdma_disconnect failed");
}

void IBConnection::on_rejected(struct rdma_cm_event* event)
{
    out.debug() << "[" << _index << "] "
                << "connection rejected";

    rdma_destroy_qp(_cm_id);
}

void IBConnection::on_established(struct rdma_cm_event* event)
{
    out.debug() << "[" << _index << "] "
                << "connection established";
}

void IBConnection::on_disconnected(struct rdma_cm_event* event)
{
    out.debug() << "[" << _index << "] "
                << "connection disconnected";

    rdma_destroy_qp(_cm_id);
}

void IBConnection::on_addr_resolved(struct ibv_pd* pd, struct ibv_cq* cq)
{
    out.debug() << "address resolved";

    struct ibv_qp_init_attr qp_attr;
    memset(&qp_attr, 0, sizeof qp_attr);
    qp_attr.cap = _qp_cap;
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;
    int err = rdma_create_qp(_cm_id, pd, &qp_attr);
    if (err)
        throw InfinibandException("creation of QP failed");

    err = rdma_resolve_route(_cm_id, RESOLVE_TIMEOUT_MS);
    if (err)
        throw InfinibandException("rdma_resolve_route failed");

    setup(pd);
}

void IBConnection::create_qp(struct ibv_pd* pd, struct ibv_cq* cq)
{
    struct ibv_qp_init_attr qp_attr;
    memset(&qp_attr, 0, sizeof qp_attr);
    qp_attr.cap = _qp_cap;
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;
    int err = rdma_create_qp(_cm_id, pd, &qp_attr);
    if (err)
        throw InfinibandException("creation of QP failed");
}

void IBConnection::accept_connect_request()
{
    out.debug() << "accepting connection";

    // Accept rdma connection request
    auto private_data = get_private_data();
    assert(private_data->size() <= 255);

    struct rdma_conn_param conn_param = {};
    conn_param.responder_resources = 1;
    conn_param.private_data = private_data->data();
    conn_param.private_data_len = static_cast<uint8_t>(private_data->size());
    int err = rdma_accept(_cm_id, &conn_param);
    if (err)
        throw InfinibandException("RDMA accept failed");
}

void IBConnection::on_connect_request(struct rdma_cm_event* event,
                                      struct ibv_pd* pd, struct ibv_cq* cq)
{
    create_qp(pd, cq);
    setup(pd);
    accept_connect_request();
}

std::unique_ptr<std::vector<uint8_t> > IBConnection::get_private_data()
{
    std::unique_ptr
        <std::vector<uint8_t> > private_data(new std::vector<uint8_t>());

    return private_data;
}

void IBConnection::on_route_resolved()
{
    out.debug() << "route resolved";

    // Initiate rdma connection
    auto private_data = get_private_data();
    assert(private_data->size() <= 255);

    struct rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof conn_param);
    conn_param.initiator_depth = 1;
    conn_param.retry_count = 7;
    conn_param.private_data = private_data->data();
    conn_param.private_data_len = static_cast<uint8_t>(private_data->size());
    int err = rdma_connect(_cm_id, &conn_param);
    if (err) {
        out.fatal() << "rdma_connect failed: " << strerror(err);
        throw InfinibandException("rdma_connect failed");
    }
}

void IBConnection::dump_send_wr(struct ibv_send_wr* wr)
{
    for (int i = 0; wr; ++i, wr = wr->next) {
        out.fatal() << "wr[" << i << "]: wr_id=" << wr->wr_id << " ("
                    << static_cast<REQUEST_ID>(wr->wr_id) << ")";
        out.fatal() << " opcode=" << wr->opcode;
        out.fatal() << " send_flags=" << static_cast
            <ibv_send_flags>(wr->send_flags);
        out.fatal() << " num_sge=" << wr->num_sge;
        for (int j = 0; j < wr->num_sge; ++j) {
            out.fatal() << "  sg_list[" << j << "] "
                        << "addr=" << wr->sg_list[j].addr;
            out.fatal() << "  sg_list[" << j << "] "
                        << "length=" << wr->sg_list[j].length;
            out.fatal() << "  sg_list[" << j << "] "
                        << "lkey=" << wr->sg_list[j].lkey;
        }
    }
}

void IBConnection::post_send(struct ibv_send_wr* wr)
{
    struct ibv_send_wr* bad_send_wr;

    int err = ibv_post_send(qp(), wr, &bad_send_wr);
    if (err) {
        out.fatal() << "ibv_post_send failed: " << strerror(err);
        dump_send_wr(wr);
        out.fatal() << "previous send requests: " << _total_send_requests;
        out.fatal() << "previous recv requests: " << _total_recv_requests;
        throw InfinibandException("ibv_post_send failed");
    }

    ++_total_send_requests;

    while (wr) {
        for (int i = 0; i < wr->num_sge; ++i)
            _total_bytes_sent += wr->sg_list[i].length;
        wr = wr->next;
    }
}

void IBConnection::post_recv(struct ibv_recv_wr* wr)
{
    struct ibv_recv_wr* bad_recv_wr;

    int err = ibv_post_recv(qp(), wr, &bad_recv_wr);
    if (err) {
        out.fatal() << "ibv_post_recv failed: " << strerror(err);
        throw InfinibandException("ibv_post_recv failed");
    }

    ++_total_recv_requests;
}
