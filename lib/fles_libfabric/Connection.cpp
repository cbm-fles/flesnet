// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "Connection.hpp"

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

  send_heartbeat_message_.info.index = remote_index_;
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

  int err = fi_getinfo(FIVERSION, hostname.empty() ? nullptr : hostname.c_str(),
                       service.empty() ? nullptr : service.c_str(),
                       FI_NUMERICHOST, hints, &info2);
  if (err != 0) {
    L_(fatal) << "fi_getinfo failed in connect: " << hostname << " " << service
              << "[" << err << "=" << fi_strerror(-err) << "]";
    throw LibfabricException("fi_getinfo failed in connect");
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
  if (ep_ != nullptr) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    fi_close((::fid_t)ep_);
#pragma GCC diagnostic pop
  }
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
                               const std::string& /*hostname*/,
                               const std::string& /*service*/,
                               struct fid_domain* pd,
                               struct fid_cq* cq,
                               struct fid_av* av) {

  struct fi_info* info2 = info;

  int err = fi_endpoint(pd, info2, &ep_, this);
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

void Connection::setup_heartbeat() {
  heartbeat_recv_descs[0] = fi_mr_desc(mr_heartbeat_recv_);
  heartbeat_send_descs[0] = fi_mr_desc(mr_heartbeat_send_);

  // setup send and receive buffers
  memset(&heartbeat_recv_wr_iovec, 0, sizeof(struct iovec));
  heartbeat_recv_wr_iovec.iov_base = &recv_heartbeat_message_;
  heartbeat_recv_wr_iovec.iov_len = sizeof(recv_heartbeat_message_);

  memset(&heartbeat_recv_wr, 0, sizeof(struct fi_msg));
  heartbeat_recv_wr.msg_iov = &heartbeat_recv_wr_iovec;
  heartbeat_recv_wr.desc = heartbeat_recv_descs;
  heartbeat_recv_wr.iov_count = 1;
  heartbeat_recv_wr.tag = ConstVariables::HEARTBEAT_MESSAGE_TAG;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  struct fi_custom_context* context =
      LibfabricContextPool::getInst()->getContext();
  context->op_context = (ID_HEARTBEAT_RECEIVE_STATUS | (index_ << 8));
  heartbeat_recv_wr.context = context;
#pragma GCC diagnostic pop

  memset(&heartbeat_send_wr_iovec, 0, sizeof(struct iovec));
  heartbeat_send_wr_iovec.iov_base = &send_heartbeat_message_;
  heartbeat_send_wr_iovec.iov_len = sizeof(send_heartbeat_message_);

  memset(&heartbeat_send_wr, 0, sizeof(struct fi_msg));
  heartbeat_send_wr.msg_iov = &heartbeat_send_wr_iovec;
  heartbeat_send_wr.desc = heartbeat_send_descs;
  heartbeat_send_wr.iov_count = 1;
  heartbeat_send_wr.tag = ConstVariables::HEARTBEAT_MESSAGE_TAG;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  context = LibfabricContextPool::getInst()->getContext();
  context->op_context = (ID_HEARTBEAT_SEND_STATUS | (index_ << 8));
  heartbeat_send_wr.context = context;
#pragma GCC diagnostic pop

  // post initial receive request
  post_recv_heartbeat_message();
}

void Connection::setup_heartbeat_mr(struct fid_domain* pd) {
  // register memory regions
  int res =
      fi_mr_reg(pd, &recv_heartbeat_message_, sizeof(recv_heartbeat_message_),
                FI_RECV | FI_TAGGED, 0, Provider::requested_key++, 0,
                &mr_heartbeat_recv_, nullptr);
  if (res != 0) {
    L_(fatal) << "fi_mr_reg failed for heartbeat recv msg: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("fi_mr_reg failed for heartbeat recv msg");
  }

  if (mr_heartbeat_recv_ == nullptr)
    throw LibfabricException(
        "registration of memory region failed in Connection");

  res = fi_mr_reg(pd, &send_heartbeat_message_, sizeof(send_heartbeat_message_),
                  FI_SEND | FI_TAGGED, 0, Provider::requested_key++, 0,
                  &mr_heartbeat_send_, nullptr);
  if (res != 0) {
    L_(fatal) << "fi_mr_reg failed for heartbeat send msg: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("fi_mr_reg failed for heartbeat send msg");
  }

  if (mr_heartbeat_send_ == nullptr)
    throw LibfabricException(
        "registration of memory region failed in Connection2");
}

void Connection::post_recv_heartbeat_message() {
  if (false) {
    L_(info) << "[i" << remote_index_ << "] "
             << "[" << index_ << "] "
             << "POST RECEIVE heartbeat message";
  }
  post_recv_msg(&heartbeat_recv_wr);
}

void Connection::post_send_heartbeat_message() {
  if (true) {
    L_(info) << "[i" << remote_index_ << "] "
             << "[" << index_ << "] "
             << "POST SEND heartbeat message ("
             << "id=" << send_heartbeat_message_.message_id
             << " ack=" << send_heartbeat_message_.ack << " failed node "
             << send_heartbeat_message_.failure_info.index << " failed desc "
             << send_heartbeat_message_.failure_info.last_completed_desc
             << " failed trigger "
             << send_heartbeat_message_.failure_info.timeslice_trigger << ")";
  }
  heartbeat_send_buffer_available_ = false;
  post_send_msg(&heartbeat_send_wr);
}

bool Connection::post_send_msg(const struct fi_msg_tagged* wr) {
  // We need only FI_INJECT_COMPLETE but this is not supported with GNI
  uint64_t flags = FI_COMPLETION;
  int err = fi_tsendmsg(ep_, wr, flags);
  if (err != 0) {
    // dump_send_wr(wr);
    L_(fatal) << "previous send requests: " << total_send_requests_;
    L_(fatal) << "previous recv requests: " << total_recv_requests_;
    L_(fatal) << "fi_sendmsg failed: " << err << "=" << fi_strerror(-err);
    // throw LibfabricException("fi_sendmsg failed");
    return false;
  }

  ++total_send_requests_;

  for (size_t i = 0; i < wr->iov_count; ++i) {
    total_bytes_sent_ += wr->msg_iov[i].iov_len;
    total_sync_bytes_sent_ += wr->msg_iov[i].iov_len;
  }
  return true;
}

/// Post an Libfabric rdma send work request
bool Connection::post_send_rdma(struct fi_msg_rma* wr, uint64_t flags) {
  int err = fi_writemsg(ep_, wr, flags);
  if (err != 0) {
    // dump_send_wr(wr);
    L_(fatal) << "previous send requests: " << total_send_requests_;
    L_(fatal) << "previous recv requests: " << total_recv_requests_;
    L_(fatal) << "fi_writemsg failed: " << err << "=" << fi_strerror(-err);
    // throw LibfabricException("fi_writemsg failed");
    return false;
  }
  ++total_send_requests_;

  for (size_t i = 0; i < wr->iov_count; ++i)
    total_bytes_sent_ += wr->msg_iov[i].iov_len;
  return true;
}

bool Connection::post_recv_msg(const struct fi_msg_tagged* wr) {
  int err = fi_trecvmsg(ep_, wr, FI_COMPLETION);
  if (err != 0) {
    L_(fatal) << "fi_recvmsg failed: " << err << "=" << fi_strerror(-err);
    // throw LibfabricException("fi_recvmsg failed");
    return false;
  }

  ++total_recv_requests_;
  return true;
}

void Connection::prepare_heartbeat(HeartbeatFailedNodeInfo* failure_info,
                                   uint64_t message_id,
                                   bool ack) {
  HeartbeatMessage* message = new HeartbeatMessage();
  if (message_id == ConstVariables::MINUS_ONE)
    message->message_id =
        SchedulerOrchestrator::get_next_heartbeat_message_id();
  else
    message->message_id = message_id;
  message->ack = ack;
  if (failure_info == nullptr) {
    message->failure_info.index = ConstVariables::MINUS_ONE;
    message->failure_info.last_completed_desc = ConstVariables::MINUS_ONE;
    message->failure_info.timeslice_trigger = ConstVariables::MINUS_ONE;
  } else {
    message->failure_info.index = failure_info->index;
    message->failure_info.last_completed_desc =
        failure_info->last_completed_desc;
    message->failure_info.timeslice_trigger = failure_info->timeslice_trigger;
  }

  if (heartbeat_send_buffer_available_) {
    send_heartbeat(message);
  } else {
    SchedulerOrchestrator::add_pending_heartbeat_message(index_, message);
  }

  // This should be moved to send_heartbeat but would cause no progress in
  // case of failure because the completion will be never received and
  // therefore, no log_sent_heartbeat_message will be called, which is
  // essential to mark connection timeout!
  if (!ack) {
    SchedulerOrchestrator::log_sent_heartbeat_message(index_, *message);
  }
  message = nullptr;
}

void Connection::send_heartbeat(HeartbeatMessage* message) {
  send_heartbeat_message_.message_id = message->message_id;
  send_heartbeat_message_.ack = message->ack;
  send_heartbeat_message_.failure_info.index = message->failure_info.index;
  send_heartbeat_message_.failure_info.last_completed_desc =
      message->failure_info.last_completed_desc;
  send_heartbeat_message_.failure_info.timeslice_trigger =
      message->failure_info.timeslice_trigger;
  post_send_heartbeat_message();
}

void Connection::on_complete_heartbeat_send() {
  if (false) {
    L_(info)
        << "[i" << remote_index_ << "] "
        << "[" << index_ << "] "
        << "on_complete_heartbeat_send, send_heartbeat_message_.message_id="
        << send_heartbeat_message_.message_id;
  }
  heartbeat_send_buffer_available_ = true;
  HeartbeatMessage* message =
      SchedulerOrchestrator::get_pending_heartbeat_message(index_);
  if (message != nullptr)
    send_heartbeat(message);
}
} // namespace tl_libfabric
