// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "Connection.hpp"

#include "LibfabricException.hpp"
#include "Provider.hpp"
//#include "RequestIdentifier.hpp"

#include <rdma/fi_cm.h>
#include <rdma/fi_rma.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <log.hpp>
#include <netdb.h>
#include <thread>

namespace tl_libfabric {

Connection::Connection(struct fid_eq* eq,
                       uint_fast16_t connection_index,
                       uint_fast16_t remote_connection_index)
    : index_(connection_index), remote_index_(remote_connection_index),
      eq_(eq) {

  // dead code?
  max_send_wr_ = 16;
  max_recv_wr_ = 16;
  max_send_sge_ = 8;
  max_recv_sge_ = 8;
  max_inline_data_ = 0;
}

Connection::~Connection() {
  if (ep_ != nullptr) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    int err = fi_close((struct fid*)ep_);
#pragma GCC diagnostic pop
    if (err != 0) {
      L_(error) << "fi_close() failed";
    }
    ep_ = nullptr;
  }
}

void Connection::connect(const std::string& hostname,
                         const std::string& service,
                         struct fid_domain* domain,
                         struct fid_cq* cq,
                         struct fid_av* av) {
  auto private_data = get_private_data();
  assert(private_data->size() <= 255);

  L_(debug) << "connect: " << hostname << ":" << service;
  struct fi_info* info2 = nullptr;
  struct fi_info* hints = fi_dupinfo(Provider::getInst()->get_info());

  hints->rx_attr->size = max_recv_wr_;
  hints->rx_attr->iov_limit = max_recv_sge_;
  // TODO this attribute causes a problem while running flesnet
  // hints->tx_attr->size = max_send_wr_;
  hints->tx_attr->iov_limit = max_send_sge_;
  hints->tx_attr->inject_size = max_inline_data_;

  hints->src_addr = nullptr;
  hints->src_addrlen = 0;

  int err = fi_getinfo(
      FI_VERSION(1, 1), hostname.empty() ? nullptr : hostname.c_str(),
      service.empty() ? nullptr : service.c_str(), 0, hints, &info2);
  if (err != 0) {
    L_(fatal) << "fi_getinfo failed in make_endpoint: " << hostname << " "
              << service << "[" << err << "=" << fi_strerror(-err) << "]";
    throw LibfabricException("fi_getinfo failed in make_endpoint");
  }

  fi_freeinfo(hints);

  err = fi_endpoint(domain, info2, &ep_, this);
  if (err != 0) {
    L_(fatal) << "fi_endpoint failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_endpoint failed");
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  if (Provider::getInst()->has_eq_at_eps()) {
    err = fi_ep_bind(ep_, (::fid_t)eq_, 0);
    if (err != 0) {
      L_(fatal) << "fi_ep_bind failed: " << err << "=" << fi_strerror(-err);
      throw LibfabricException("fi_ep_bind failed");
    }
  }
  err =
      fi_ep_bind(ep_, (::fid_t)cq, FI_SEND | FI_RECV | FI_SELECTIVE_COMPLETION);
  if (err != 0) {
    L_(fatal) << "fi_ep_bind failed (cq): " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_ep_bind failed (cq)");
  }
  if (Provider::getInst()->has_av()) {
    err = fi_ep_bind(ep_, (::fid_t)av, 0);
    if (err != 0) {
      L_(fatal) << "fi_ep_bind failed (av): " << err << "="
                << fi_strerror(-err);
      throw LibfabricException("fi_ep_bind failed (av)");
    }
  }
#pragma GCC diagnostic pop
  err = fi_enable(ep_);
  if (err != 0) {
    L_(fatal) << "fi_enable failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_enable failed");
  }

  setup_mr(domain);
  Provider::getInst()->connect(ep_, max_send_wr_, max_send_sge_, max_recv_wr_,
                               max_recv_sge_, max_inline_data_,
                               private_data->data(), private_data->size(),
                               info2->dest_addr);
  setup();
}

void Connection::disconnect() {
  L_(debug) << "[" << index_ << "] "
            << "disconnect";
  int err = fi_shutdown(ep_, 0);
  if (err != 0) {
    L_(fatal) << "fi_shutdown failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_shutdown failed");
  }
}

void Connection::on_rejected(struct fi_eq_err_entry* /* event */) {
  L_(debug) << "[" << index_ << "] "
            << "connection rejected";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  fi_close((::fid_t)ep_);
#pragma GCC diagnostic pop

  ep_ = nullptr;
}

void Connection::on_established(struct fi_eq_cm_entry* /* event */) {
  L_(debug) << "[" << index_ << "] "
            << "connection established";
}

void Connection::on_disconnected(struct fi_eq_cm_entry* /* event */) {
  L_(debug) << "[" << index_ << "] "
            << "connection disconnected";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  fi_close((::fid_t)ep_);
#pragma GCC diagnostic pop

  ep_ = nullptr;
}

void Connection::on_connect_request(struct fi_eq_cm_entry* event,
                                    struct fid_domain* pd,
                                    struct fid_cq* cq) {
  int err = fi_endpoint(pd, event->info, &ep_, this);
  if (err != 0) {
    L_(fatal) << "fi_endpoint failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_endpoint failed");
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  err = fi_ep_bind(ep_, (::fid_t)eq_, 0);
  if (err != 0) {
    L_(fatal) << "fi_ep_bind failed to eq: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_ep_bind failed to eq");
  }
  err = fi_ep_bind(ep_, (fid_t)cq, FI_SEND | FI_RECV | FI_SELECTIVE_COMPLETION);
  if (err != 0) {
    L_(fatal) << "fi_ep_bind failed to cq: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_ep_bind failed to cq");
  }
#pragma GCC diagnostic pop

  // setup(pd);
  setup_mr(pd);

  auto private_data = get_private_data();
  assert(private_data->size() <= 255);

  err = fi_enable(ep_);
  if (err != 0) {
    L_(fatal) << "fi_enable failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_enable failed");
  }
  // accept_connect_request();
  err = fi_accept(ep_, private_data->data(), private_data->size());
  if (err != 0) {
    L_(fatal) << "fi_accept failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_accept failed");
  }

  // setup(pd);
  setup();
}

void Connection::make_endpoint(struct fi_info* info,
                               const std::string& hostname,
                               const std::string& service,
                               struct fid_domain* pd,
                               struct fid_cq* cq,
                               struct fid_av* av) {

  struct fi_info* info2 = nullptr;
  struct fi_info* hints = fi_dupinfo(info);

  hints->rx_attr->size = max_recv_wr_;
  hints->rx_attr->iov_limit = max_recv_sge_;
  hints->tx_attr->size = max_send_wr_;
  hints->tx_attr->iov_limit = max_send_sge_;
  hints->tx_attr->inject_size = max_inline_data_;

  hints->src_addr = nullptr;
  hints->src_addrlen = 0;

  int err = fi_getinfo(
      FI_VERSION(1, 1), hostname.empty() ? nullptr : hostname.c_str(),
      service.empty() ? nullptr : service.c_str(), 0, hints, &info2);
  if (err != 0) {
    L_(fatal) << "fi_getinfo failed in make_endpoint: " << err << "="
              << fi_strerror(-err);
    throw LibfabricException("fi_getinfo failed in make_endpoint");
  }

  fi_freeinfo(hints);

  err = fi_endpoint(pd, info2, &ep_, this);
  if (err != 0) {
    L_(fatal) << "fi_endpoint failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_endpoint failed");
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  if (Provider::getInst()->has_eq_at_eps()) {
    err = fi_ep_bind(ep_, (::fid_t)eq_, 0);
    if (err != 0) {
      L_(fatal) << "fi_ep_bind failed (eq_): " << err << "="
                << fi_strerror(-err);
      throw LibfabricException("fi_ep_bind failed (eq_)");
    }
  }
  err =
      fi_ep_bind(ep_, (::fid_t)cq, FI_SEND | FI_RECV | FI_SELECTIVE_COMPLETION);
  if (err != 0) {
    L_(fatal) << "fi_ep_bind failed (cq): " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_ep_bind failed (cq)");
  }
  if (Provider::getInst()->has_av()) {
    err = fi_ep_bind(ep_, (::fid_t)av, 0);
    if (err != 0) {
      L_(fatal) << "fi_ep_bind failed (av): " << err << "="
                << fi_strerror(-err);
      throw LibfabricException("fi_ep_bind failed (av)");
    }
  }
#pragma GCC diagnostic pop
  err = fi_enable(ep_);
  if (err != 0) {
    L_(fatal) << "fi_enable failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_enable failed");
  }
}

std::unique_ptr<std::vector<uint8_t>> Connection::get_private_data() {
  std::unique_ptr<std::vector<uint8_t>> private_data(
      new std::vector<uint8_t>());

  return private_data;
}

void Connection::post_send_msg(struct fi_msg* wr) {

  int err = fi_sendmsg(ep_, wr, FI_COMPLETION);
  if (err != 0) {
    // dump_send_wr(wr);
    L_(fatal) << "previous send requests: " << total_send_requests_;
    L_(fatal) << "previous recv requests: " << total_recv_requests_;
    L_(fatal) << "fi_sendmsg failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_sendmsg failed");
  }

  ++total_send_requests_;

  for (size_t i = 0; i < wr->iov_count; ++i) {
    total_bytes_sent_ += wr->msg_iov[i].iov_len;
  }
}

/// Post an Libfabric rdma send work request
void Connection::post_send_rdma(struct fi_msg_rma* wr, uint64_t flags) {
  int err = fi_writemsg(ep_, wr, flags);
  if (err != 0) {
    // dump_send_wr(wr);
    L_(fatal) << "previous send requests: " << total_send_requests_;
    L_(fatal) << "previous recv requests: " << total_recv_requests_;
    L_(fatal) << "fi_writemsg failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_writemsg failed");
  }
  ++total_send_requests_;

  for (size_t i = 0; i < wr->iov_count; ++i) {
    total_bytes_sent_ += wr->msg_iov[i].iov_len;
  }
}

void Connection::post_recv_msg(const struct fi_msg* wr) {
  int err = fi_recvmsg(ep_, wr, FI_COMPLETION);
  if (err != 0) {
    L_(fatal) << "fi_recvmsg failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_recvmsg failed");
  }

  ++total_recv_requests_;
}
} // namespace tl_libfabric
