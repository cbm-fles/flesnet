// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "IBConnection.hpp"
#include "RequestIdentifier.hpp"
#include "log.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <netdb.h>
#include <thread>

IBConnection::IBConnection(struct rdma_event_channel* ec,
                           uint_fast16_t connection_index,
                           uint_fast16_t remote_connection_index,
                           struct rdma_cm_id* id)
    : index_(connection_index), remote_index_(remote_connection_index),
      cm_id_(id) {
  if (cm_id_ == nullptr) {
    int err = rdma_create_id(ec, &cm_id_, this, RDMA_PS_TCP);
    if (err != 0) {
      throw InfinibandException("rdma_create_id failed");
    }
  } else {
    cm_id_->context = this;
  }

  qp_cap_.max_send_wr = 16;
  qp_cap_.max_recv_wr = 16;
  qp_cap_.max_send_sge = 8;
  qp_cap_.max_recv_sge = 8;
  qp_cap_.max_inline_data = 0;
}

IBConnection::~IBConnection() {
  if (cm_id_ != nullptr) {
    int err = rdma_destroy_id(cm_id_);
    if (err != 0) {
      L_(error) << "rdma_destroy_id() failed";
    }
    cm_id_ = nullptr;
  }
}

void IBConnection::connect(const std::string& hostname,
                           const std::string& service) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res;

  int err = getaddrinfo(hostname.c_str(), service.c_str(), &hints, &res);
  if (err != 0) {
    throw InfinibandException("getaddrinfo failed");
  }

  L_(debug) << "[" << index_ << "] "
            << "resolution of server address and route";

  for (struct addrinfo* t = res; t != nullptr; t = t->ai_next) {
    err = rdma_resolve_addr(cm_id_, nullptr, t->ai_addr, RESOLVE_TIMEOUT_MS);
    if (err == 0) {
      break;
    }
  }
  if (err != 0) {
    L_(fatal) << "rdma_resolve_addr failed: " << strerror(errno);
    throw InfinibandException("rdma_resolve_addr failed");
  }

  freeaddrinfo(res);
}

void IBConnection::disconnect() {
  L_(debug) << "[" << index_ << "] "
            << "disconnect";
  int err = rdma_disconnect(cm_id_);
  if (err != 0) {
    throw InfinibandException("rdma_disconnect failed");
  }
}

void IBConnection::on_rejected(struct rdma_cm_event* /* event */) {
  L_(debug) << "[" << index_ << "] "
            << "connection rejected";

  rdma_destroy_qp(cm_id_);
}

void IBConnection::on_established(struct rdma_cm_event* /* event */) {
  L_(debug) << "[" << index_ << "] "
            << "connection established";
}

void IBConnection::on_disconnected(struct rdma_cm_event* /* event */) {
  L_(debug) << "[" << index_ << "] "
            << "connection disconnected";
}

void IBConnection::on_timewait_exit(struct rdma_cm_event* /* event */) {
  L_(debug) << "[" << index_ << "] "
            << "connection reached timewait_exit";

  rdma_destroy_qp(cm_id_);
}

void IBConnection::on_addr_resolved(struct ibv_pd* pd, struct ibv_cq* cq) {
  L_(debug) << "address resolved";

  struct ibv_qp_init_attr qp_attr;
  memset(&qp_attr, 0, sizeof qp_attr);
  qp_attr.cap = qp_cap_;
  qp_attr.send_cq = cq;
  qp_attr.recv_cq = cq;
  qp_attr.qp_type = IBV_QPT_RC;
  int err = rdma_create_qp(cm_id_, pd, &qp_attr);
  if (err != 0) {
    throw InfinibandException("creation of QP failed");
  }

  err = rdma_resolve_route(cm_id_, RESOLVE_TIMEOUT_MS);
  if (err != 0) {
    throw InfinibandException("rdma_resolve_route failed");
  }

  setup(pd);
}

void IBConnection::create_qp(struct ibv_pd* pd, struct ibv_cq* cq) {
  struct ibv_qp_init_attr qp_attr;
  memset(&qp_attr, 0, sizeof qp_attr);
  qp_attr.cap = qp_cap_;
  qp_attr.send_cq = cq;
  qp_attr.recv_cq = cq;
  qp_attr.qp_type = IBV_QPT_RC;
  int err = rdma_create_qp(cm_id_, pd, &qp_attr);
  if (err != 0) {
    throw InfinibandException("creation of QP failed");
  }
}

void IBConnection::accept_connect_request() {
  L_(debug) << "accepting connection";

  // Accept rdma connection request
  auto private_data = get_private_data();
  assert(private_data->size() <= 255);

  struct rdma_conn_param conn_param = rdma_conn_param();
  conn_param.responder_resources = 1;
  conn_param.private_data = private_data->data();
  conn_param.private_data_len = static_cast<uint8_t>(private_data->size());
  int err = rdma_accept(cm_id_, &conn_param);
  if (err != 0) {
    throw InfinibandException("RDMA accept failed");
  }
}

void IBConnection::on_connect_request(struct rdma_cm_event* /* event */,
                                      struct ibv_pd* pd,
                                      struct ibv_cq* cq) {
  create_qp(pd, cq);
  setup(pd);
  accept_connect_request();
}

std::unique_ptr<std::vector<uint8_t>> IBConnection::get_private_data() {
  std::unique_ptr<std::vector<uint8_t>> private_data(
      new std::vector<uint8_t>());

  return private_data;
}

void IBConnection::on_route_resolved() {
  L_(debug) << "route resolved";

  // Initiate rdma connection
  auto private_data = get_private_data();
  assert(private_data->size() <= 255);

  struct rdma_conn_param conn_param = rdma_conn_param();
  conn_param.initiator_depth = 1;
  conn_param.retry_count = 7;
  conn_param.private_data = private_data->data();
  conn_param.private_data_len = static_cast<uint8_t>(private_data->size());
  // TODO: Hack to prevent connection issues when using softiwarp.
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  int err = rdma_connect(cm_id_, &conn_param);
  if (err != 0) {
    L_(fatal) << "rdma_connect failed: " << strerror(errno);
    throw InfinibandException("rdma_connect failed");
  }
}

void IBConnection::dump_send_wr(struct ibv_send_wr* wr) {
  for (int i = 0; wr != nullptr; ++i, wr = wr->next) {
    L_(fatal) << "wr[" << i << "]: wr_id=" << wr->wr_id << " ("
              << static_cast<RequestIdentifier>(wr->wr_id) << ")";
    L_(fatal) << " opcode=" << wr->opcode;
    L_(fatal) << " send_flags=" << static_cast<ibv_send_flags>(wr->send_flags);
    L_(fatal) << " num_sge=" << wr->num_sge;
    for (int j = 0; j < wr->num_sge; ++j) {
      L_(fatal) << "  sg_list[" << j << "] "
                << "addr=" << wr->sg_list[j].addr;
      L_(fatal) << "  sg_list[" << j << "] "
                << "length=" << wr->sg_list[j].length;
      L_(fatal) << "  sg_list[" << j << "] "
                << "lkey=" << wr->sg_list[j].lkey;
    }
  }
}

void IBConnection::post_send(struct ibv_send_wr* wr) {
  struct ibv_send_wr* bad_send_wr;

  int err = ibv_post_send(qp(), wr, &bad_send_wr);
  if (err != 0) {
    L_(fatal) << "ibv_post_send failed: " << strerror(err);
    dump_send_wr(wr);
    L_(fatal) << "previous send requests: " << total_send_requests_;
    L_(fatal) << "previous recv requests: " << total_recv_requests_;
    throw InfinibandException("ibv_post_send failed");
  }

  ++total_send_requests_;

  while (wr != nullptr) {
    for (int i = 0; i < wr->num_sge; ++i) {
      total_bytes_sent_ += wr->sg_list[i].length;
    }
    wr = wr->next;
  }
}

void IBConnection::post_recv(struct ibv_recv_wr* wr) {
  struct ibv_recv_wr* bad_recv_wr;

  int err = ibv_post_recv(qp(), wr, &bad_recv_wr);
  if (err != 0) {
    L_(fatal) << "ibv_post_recv failed: " << strerror(err);
    throw InfinibandException("ibv_post_recv failed");
  }

  ++total_recv_requests_;
}
