// Copyright 2012-2014 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "InputChannelConnection.hpp"

namespace tl_libfabric {

InputChannelConnection::InputChannelConnection(
    struct fid_eq* eq,
    uint_fast16_t connection_index,
    uint_fast16_t remote_connection_index,
    unsigned int max_send_wr,
    unsigned int max_pending_write_requests)
    : Connection(eq, connection_index, remote_connection_index),
      max_pending_write_requests_(max_pending_write_requests) {
  assert(max_pending_write_requests_ > 0);

  max_send_wr_ = max_send_wr; // typical hca maximum: 16k
  max_send_sge_ = 4;          // max. two chunks each for descriptors and data

  max_recv_wr_ = 1; // receive only single ComputeNodeStatusMessage struct
  max_recv_sge_ = 1;

  max_inline_data_ = sizeof(fles::TimesliceComponentDescriptor);

  send_status_message_.info.index = remote_index_;

  if (Provider::getInst()->is_connection_oriented()) {
    connection_oriented_ = true;
  } else {
    connection_oriented_ = false;
  }

  data_changed_ = true; // to send empty message at the beginning
  data_acked_ = false;  // to send empty message at the beginning
  msg_latency_.resize(ConstVariables::MAX_MEDIAN_VALUES);
}

bool InputChannelConnection::check_for_buffer_space(uint64_t data_size,
                                                    uint64_t desc_size) {
  if (false) {
    L_(trace) << "[" << index_ << "] "
              << "SENDER data space (bytes) required=" << data_size
              << ", avail="
              << cn_ack_.data +
                     (UINT64_C(1) << remote_info_.data_buffer_size_exp) -
                     cn_wp_.data - cn_wp_pending_.data;
    L_(trace) << "[" << index_ << "] "
              << "SENDER desc space (entries) required=" << desc_size
              << ", avail="
              << cn_ack_.desc +
                     (UINT64_C(1) << remote_info_.desc_buffer_size_exp) -
                     cn_wp_.desc - cn_wp_pending_.desc;
  }

  if (cn_ack_.data - cn_wp_.data - cn_wp_pending_.data +
              (UINT64_C(1) << remote_info_.data_buffer_size_exp) <
          data_size ||
      cn_ack_.desc - cn_wp_.desc - cn_wp_pending_.desc +
              (UINT64_C(1) << remote_info_.desc_buffer_size_exp) <
          desc_size) { // TODO: extend condition!
    return false;
  }
  return true;
}

bool InputChannelConnection::send_data(struct iovec* sge,
                                       void** desc,
                                       int num_sge,
                                       uint64_t timeslice,
                                       uint64_t desc_length,
                                       uint64_t data_length,
                                       uint64_t skip) {
  std::pair<uint64_t, uint64_t> last_timeslice_info =
      InputSchedulerOrchestrator::get_data_and_desc_of_last_timeslice(index_);
  int num_sge2 = 0;
  struct iovec sge2[4];
  void* desc2[4];
  bool res = true;

  uint64_t cn_wp_data = cn_wp_.data + cn_wp_pending_.data;
  if (last_timeslice_info.first != cn_wp_data) {
    L_(fatal) << "Incorrect cn_wp_data before sending timeslice " << timeslice
              << " to " << index_ << "--------- ts " << timeslice
              << " cn_wp_.data = " << cn_wp_.data
              << " cn_wp_.desc = " << cn_wp_.desc
              << " cn_wp_pending_.data = " << cn_wp_pending_.data
              << " cn_wp_pending_.desc = " << cn_wp_pending_.desc
              << " last_timeslice_info.first = " << last_timeslice_info.first
              << " last_timeslice_info.second = " << last_timeslice_info.second;

    assert(false);
  }
  cn_wp_data += skip;

  uint64_t cn_data_buffer_mask =
      (UINT64_C(1) << remote_info_.data_buffer_size_exp) - 1;
  uint64_t target_bytes_left =
      (UINT64_C(1) << remote_info_.data_buffer_size_exp) -
      (cn_wp_data & cn_data_buffer_mask);

  // split sge list if necessary
  int num_sge_cut = 0;
  if (data_length + desc_length * sizeof(fles::MicrosliceDescriptor) >
      target_bytes_left) {
    for (int i = 0; i < num_sge; ++i) {
      if (sge[i].iov_len <= target_bytes_left) {
        target_bytes_left -= sge[i].iov_len;
      } else {
        if (target_bytes_left != 0u) {
          sge2[num_sge2].iov_base =
              reinterpret_cast<uint8_t*>(sge[i].iov_base) + target_bytes_left;
          sge2[num_sge2].iov_len = sge[i].iov_len - target_bytes_left;
          desc2[num_sge2++] = desc[i];
          sge[i].iov_len = target_bytes_left;
          target_bytes_left = 0;
        } else {
          sge2[num_sge2] = sge[i];
          desc2[num_sge2++] = desc[i];
          ++num_sge_cut;
        }
      }
    }
  }
  num_sge -= num_sge_cut;

  struct fi_msg_rma send_wr_ts;
  struct fi_msg_rma send_wr_tswrap;
  struct fi_rma_iov rma_iov[1];
  struct fi_custom_context* context;

  uint64_t remote_addr =
      remote_info_.data.addr + (cn_wp_data & cn_data_buffer_mask);
  for (int i = 0; i < num_sge; i++) {

    memset(rma_iov, 0, sizeof(rma_iov));
    rma_iov[0].addr = remote_addr;
    rma_iov[0].len = sge[i].iov_len;
    rma_iov[0].key = remote_info_.data.rkey;

    remote_addr += sge[i].iov_len;

    memset(&send_wr_ts, 0, sizeof(send_wr_ts));
    send_wr_ts.msg_iov = &sge[i];
    send_wr_ts.desc = &desc[i];
    send_wr_ts.iov_count = 1;
    // addr
    send_wr_ts.rma_iov = rma_iov;
    send_wr_ts.rma_iov_count = 1;
    send_wr_ts.addr = partner_addr_;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    context = LibfabricContextPool::getInst()->getContext();
    context->op_context = (ID_WRITE_DATA | (timeslice << 24) | (index_ << 8));
    send_wr_ts.context = context;
#pragma GCC diagnostic pop
    if (i + 1 < num_sge || num_sge2 > 0) {
      res = post_send_rdma(&send_wr_ts, FI_MORE);
    } else {
      res = post_send_rdma(&send_wr_ts,
                           FI_FENCE | FI_DELIVERY_COMPLETE | FI_COMPLETION);
      ++pending_write_requests_;
    }
  }

  if (num_sge2 && res) {
    uint64_t remote_addr = remote_info_.data.addr;
    for (int i = 0; i < num_sge2; i++) {

      rma_iov[0].addr = remote_addr;
      rma_iov[0].len = sge2[i].iov_len;
      rma_iov[0].key = remote_info_.data.rkey;

      remote_addr += +sge2[i].iov_len;

      memset(&send_wr_tswrap, 0, sizeof(send_wr_tswrap));
      send_wr_tswrap.msg_iov = &sge2[i];
      send_wr_tswrap.desc = &desc2[i];
      send_wr_tswrap.iov_count = 1;
      // addr
      send_wr_tswrap.rma_iov = rma_iov;
      send_wr_tswrap.rma_iov_count = 1;
      send_wr_tswrap.addr = partner_addr_;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
      context = LibfabricContextPool::getInst()->getContext();
      context->op_context =
          (ID_WRITE_DATA_WRAP | (timeslice << 24) | (index_ << 8));
      send_wr_tswrap.context = context;
#pragma GCC diagnostic pop
      if (i + 1 < num_sge2) {
        res = post_send_rdma(&send_wr_tswrap, FI_MORE);
      } else {
        res = post_send_rdma(&send_wr_tswrap,
                             FI_FENCE | FI_DELIVERY_COMPLETE | FI_COMPLETION);
        ++pending_write_requests_;
      }
    }
  }
  if (!res)
    return false;

  // timeslice component descriptor
  fles::TimesliceComponentDescriptor tscdesc;
  tscdesc.ts_num = timeslice;
  // tscdesc.ts_desc = (cn_wp_pending_.desc+ cn_wp_.desc);
  tscdesc.offset = cn_wp_data;
  tscdesc.size = data_length + desc_length * sizeof(fles::MicrosliceDescriptor);
  tscdesc.num_microslices = desc_length;

  if (false) {
    L_(info) << "[i" << remote_index_ << "] "
             << "[" << index_ << "] "
             << "POST SEND data (timeslice " << timeslice << ")"
             << " tscdesc.ts_num " << tscdesc.ts_num << " tscdesc.ts_desc "
             << last_timeslice_info.second << " tscdesc.offset "
             << tscdesc.offset << " tscdesc.size " << tscdesc.size
             << " tscdesc.num_microslices " << tscdesc.num_microslices
             << " cn_wp_.data " << cn_wp_.data << " cn_wp_pending_.data "
             << cn_wp_pending_.data << " skip " << skip;
  }
  pending_descriptors_.add(last_timeslice_info.second, tscdesc);
  assert(pending_write_requests_ <= max_pending_write_requests_);
  return true;
}

bool InputChannelConnection::write_request_available() {
  return (pending_write_requests_ < max_pending_write_requests_);
}

void InputChannelConnection::inc_write_pointers(uint64_t data_size,
                                                uint64_t desc_size) {
  cn_wp_.data += data_size;
  cn_wp_.desc += desc_size;
  data_changed_ = true;
}

void InputChannelConnection::check_inc_write_pointers() {
  uint64_t timeslice;
  fles::TimesliceComponentDescriptor descriptor;
  while (!timeslice_data_address_.empty() &&
         added_sent_descriptors_ < ConstVariables::MAX_DESCRIPTOR_ARRAY_SIZE) {

    timeslice = pending_descriptors_.get_begin_iterator()->first;
    descriptor = pending_descriptors_.get_begin_iterator()->second;
    if (!InputSchedulerOrchestrator::is_timeslice_rdma_acked(index_,
                                                             descriptor.ts_num))
      break;
    send_status_message_.tscdesc_msg[added_sent_descriptors_++] =
        std::make_pair(timeslice, descriptor);
    inc_write_pointers(timeslice_data_address_[0], 1);
    cn_wp_pending_.data -= timeslice_data_address_[0];
    cn_wp_pending_.desc -= 1;
    timeslice_data_address_.erase(timeslice_data_address_.begin());
    assert(pending_descriptors_.remove(timeslice));
    data_acked_ = true;
  }
}

bool InputChannelConnection::try_sync_buffer_positions() {
  if (!send_buffer_available_ ||
      InputSchedulerOrchestrator::is_connection_timed_out(index()))
    return false;
  if (sync_after_scheduling_decision_) {
    send_status_message_.sync_after_scheduling_decision = true;
    send_status_message_.failed_index = sync_failed_conn_;
    sync_after_scheduling_decision_ = false;
  }
  if ((get_partner_addr() || connection_oriented_) && finalize_ &&
      (!send_status_message_.final || send_status_message_.abort != abort_)) {
    if ((cn_wp_ == send_status_message_.wp) && (cn_wp_ == cn_ack_ || abort_)) {
      send_status_message_.final = true;
      send_status_message_.abort = abort_;
      data_changed_ = true;
    }
  }
  check_inc_write_pointers();
  if (data_changed_ || data_acked_ ||
      send_status_message_.sync_after_scheduling_decision) { //
    send_status_message_.wp = cn_wp_;
    send_status_message_.local_time = std::chrono::high_resolution_clock::now();
    post_send_status_message();
    return true;
  }
  return false;
}

uint64_t InputChannelConnection::skip_required(uint64_t data_size) {
  uint64_t databuf_size = UINT64_C(1) << remote_info_.data_buffer_size_exp;
  uint64_t databuf_wp =
      (cn_wp_.data + cn_wp_pending_.data) & (databuf_size - 1);
  if (databuf_wp + data_size <= databuf_size) {
    return 0;
  }
  return databuf_size - databuf_wp;
}

void InputChannelConnection::finalize(bool abort) {
  finalize_ = true;
  abort_ = abort;
  data_changed_ = true;
}

void InputChannelConnection::on_complete_write() { pending_write_requests_--; }

void InputChannelConnection::on_complete_send() {
  if (false) {
    L_(info) << "[i" << remote_index_ << "] "
             << "[" << index_ << "] "
             << "on_complete_send, send_status_message_.wp.desc="
             << send_status_message_.wp.desc
             << " new send_status_message_.wp.data="
             << send_status_message_.wp.data;
  }
  send_buffer_available_ = true;
  send_status_message_.sync_after_scheduling_decision = false;
  msg_latency_[msg_latency_index_] =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now() - msg_send_time_)
          .count();
  msg_latency_index_ = (msg_latency_index_ + 1) % msg_latency_.size();
}

void InputChannelConnection::on_complete_recv() {
  if (false) {
    L_(info)
        << "[i" << remote_index_ << "] "
        << "[" << index_ << "] "
        << "receive completion, new cn_ack_.desc="
        << recv_status_message_.ack.desc
        << " new cn_ack_.data=" << recv_status_message_.ack.data
        << " old cn_ack_.data=" << cn_ack_.data
        << " old cn_ack_.desc=" << cn_ack_.desc << "(interval index = "
        << recv_status_message_.proposed_interval_metadata.interval_index
        << " for "
        << recv_status_message_.proposed_interval_metadata.interval_duration
        << " finalize " << recv_status_message_.final;
  }

  if (recv_status_message_.final ||
      InputSchedulerOrchestrator::is_connection_timed_out(index_)) {
    done_ = true;
    return;
  }

  if (recv_status_message_.ack.desc > send_status_message_.wp.desc) {
    L_(warning) << "[i" << remote_index_ << "] "
                << "[" << index_ << "] "
                << "receive completion, unmatched new cn_ack_.desc="
                << recv_status_message_.ack.desc << " latest send status "
                << send_status_message_.wp.desc;
    assert(false);
  }

  if (recv_status_message_.ack.desc <= send_status_message_.wp.desc &&
      cn_ack_.data < recv_status_message_.ack.data &&
      cn_ack_.desc < recv_status_message_.ack.desc) {
    cn_ack_ = recv_status_message_.ack;
  }
  if (recv_status_message_.proposed_interval_metadata.interval_index !=
      ConstVariables::MINUS_ONE) {
    InputSchedulerOrchestrator::add_proposed_meta_data(
        recv_status_message_.proposed_interval_metadata);
  }
  InputSchedulerOrchestrator::log_heartbeat(index_);

  post_recv_status_message();
}

void InputChannelConnection::setup_mr(struct fid_domain* pd) {
  setup_heartbeat_mr(pd);
  // register memory regions
  int err = fi_mr_reg(pd, &recv_status_message_,
                      sizeof(ComputeNodeStatusMessage), FI_RECV | FI_TAGGED, 0,
                      Provider::requested_key++, 0, &mr_recv_, nullptr);
  if (err != 0) {
    L_(fatal) << "fi_mr_reg failed for recv msg: " << err << "="
              << fi_strerror(-err);
    throw LibfabricException("fi_mr_reg failed for recv msg");
  }

  if (mr_recv_ == nullptr)
    throw LibfabricException(
        "registration of memory region failed in InputChannelConnection");

  err = fi_mr_reg(pd, &send_status_message_, sizeof(send_status_message_),
                  FI_SEND | FI_TAGGED, 0, Provider::requested_key++, 0,
                  &mr_send_, nullptr);
  if (err != 0) {
    L_(fatal) << "fi_mr_reg failed for send msg: " << err << "="
              << fi_strerror(-err);
    throw LibfabricException("fi_mr_reg failed for send msg");
  }

  if (mr_send_ == nullptr)
    throw LibfabricException(
        "registration of memory region failed in InputChannelConnection2");
}

void InputChannelConnection::setup() {
  setup_heartbeat();
  struct fi_custom_context* context;

  recv_descs[0] = fi_mr_desc(mr_recv_);
  send_descs[0] = fi_mr_desc(mr_send_);

  // setup send and receive buffers
  memset(&recv_wr_iovec, 0, sizeof(struct iovec));
  recv_wr_iovec.iov_base = &recv_status_message_;
  recv_wr_iovec.iov_len = sizeof(ComputeNodeStatusMessage);

  memset(&recv_wr, 0, sizeof(struct fi_msg));
  recv_wr.msg_iov = &recv_wr_iovec;
  recv_wr.desc = recv_descs;
  recv_wr.iov_count = 1;
  recv_wr.tag = ConstVariables::STATUS_MESSAGE_TAG;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  context = LibfabricContextPool::getInst()->getContext();
  context->op_context = (ID_RECEIVE_STATUS | (index_ << 8));
  recv_wr.context = context;
#pragma GCC diagnostic pop

  memset(&send_wr_iovec, 0, sizeof(struct iovec));
  send_wr_iovec.iov_base = &send_status_message_;
  send_wr_iovec.iov_len = sizeof(send_status_message_);

  memset(&send_wr, 0, sizeof(struct fi_msg));
  send_wr.msg_iov = &send_wr_iovec;
  send_wr.desc = send_descs;
  send_wr.iov_count = 1;
  send_wr.tag = ConstVariables::STATUS_MESSAGE_TAG;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  context = LibfabricContextPool::getInst()->getContext();
  context->op_context = (ID_SEND_STATUS | (index_ << 8));
  send_wr.context = context;
#pragma GCC diagnostic pop

  // post initial receive request
  post_recv_status_message();
}

/// Connection handler function, called on successful connection.
/**
 \param event RDMA connection manager event structure
 */
void InputChannelConnection::on_established(struct fi_eq_cm_entry* event) {
  // assert(event->param.conn.private_data_len >= sizeof(ComputeNodeInfo));
  memcpy(&remote_info_, event->data, sizeof(ComputeNodeInfo));

  Connection::on_established(event);
}

void InputChannelConnection::dereg_mr() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  if (mr_recv_ != nullptr) {
    fi_close((struct fid*)mr_recv_);
    mr_recv_ = nullptr;
  }

  if (mr_send_ != nullptr) {
    fi_close((struct fid*)mr_send_);
    mr_send_ = nullptr;
  }
#pragma GCC diagnostic pop
}

void InputChannelConnection::on_rejected(struct fi_eq_err_entry* event) {
  L_(debug) << "InputChannelConnection:on_rejected";
  dereg_mr();
  Connection::on_rejected(event);
}

void InputChannelConnection::on_disconnected(struct fi_eq_cm_entry* event) {
  dereg_mr();
  Connection::on_disconnected(event);
}

std::unique_ptr<std::vector<uint8_t>>
InputChannelConnection::get_private_data() {
  std::unique_ptr<std::vector<uint8_t>> private_data(
      new std::vector<uint8_t>(sizeof(InputNodeInfo)));

  InputNodeInfo* in_info =
      reinterpret_cast<InputNodeInfo*>(private_data->data());
  in_info->index = remote_index_;

  return private_data;
}

void InputChannelConnection::post_recv_status_message() {
  if (false) {
    L_(trace) << "[i" << remote_index_ << "] "
              << "[" << index_ << "] "
              << "POST RECEIVE status message";
  }
  post_recv_msg(&recv_wr);
}

void InputChannelConnection::post_send_status_message() {
  if (false) {
    L_(info) << "[i" << remote_index_ << "] "
             << "[" << index_ << "] "
             << "POST SEND status message (wp.data="
             << send_status_message_.wp.data
             << " wp.desc=" << send_status_message_.wp.desc << ")"
             << " added descriptors=" << std::to_string(added_sent_descriptors_)
             << " remaining=" << pending_descriptors_.size() << " finalize "
             << send_status_message_.final;
  }

  send_status_message_.descriptor_count = added_sent_descriptors_;

  if (post_send_msg(&send_wr)) {
    data_changed_ = false;
    data_acked_ = false;
    send_buffer_available_ = false;
    added_sent_descriptors_ = 0;
    msg_send_time_ = std::chrono::high_resolution_clock::now();
  }
}

void InputChannelConnection::connect(const std::string& hostname,
                                     const std::string& service,
                                     struct fid_domain* domain,
                                     struct fid_cq* cq,
                                     struct fid_av* av,
                                     fi_addr_t fi_addr) {
  Connection::connect(hostname, service, domain, cq, av);
  if (not Provider::getInst()->is_connection_oriented()) {
    size_t addr_len = sizeof(send_status_message_.my_address);
    send_status_message_.connect = true;
    int res =
        fi_getname(&ep_->fid, &send_status_message_.my_address, &addr_len);
    assert(res == 0);
    L_(debug) << "fi_addr: " << fi_addr;
    // @todo is this save? does post_send_status_message create copy?
    send_wr.addr = fi_addr;
    heartbeat_send_wr.addr = fi_addr;
    post_send_status_message();
  }
  L_(debug) << "Calling add_endpoint in setup";
  assert(LibfabricBarrier::get_instance() != nullptr);
  LibfabricBarrier::get_instance()->add_endpoint(
      index_, Provider::getInst()->get_info(), hostname, true);
}

void InputChannelConnection::reconnect() {
  if (not Provider::getInst()->is_connection_oriented()) {
    post_send_status_message();
  }
}

void InputChannelConnection::set_partner_addr(struct fid_av* av) {

  int res = fi_av_insert(av, &this->recv_status_message_.my_address, 1,
                         &this->partner_addr_, 0, NULL);
  send_wr.addr = this->partner_addr_;
  heartbeat_send_wr.addr = this->partner_addr_;
  send_status_message_.connect = false;
  assert(res == 1);
}

fi_addr_t InputChannelConnection::get_partner_addr() {
  return this->partner_addr_;
}

void InputChannelConnection::set_remote_info() {
  this->remote_info_.data.addr = this->recv_status_message_.info.data.addr;
  this->remote_info_.data.rkey = this->recv_status_message_.info.data.rkey;

  this->remote_info_.desc.addr = this->recv_status_message_.info.desc.addr;
  this->remote_info_.desc.rkey = this->recv_status_message_.info.desc.rkey;

  this->remote_info_.data_buffer_size_exp =
      this->recv_status_message_.info.data_buffer_size_exp;

  this->remote_info_.desc_buffer_size_exp =
      this->recv_status_message_.info.desc_buffer_size_exp;

  this->remote_info_.index = this->recv_status_message_.info.index;
}

void InputChannelConnection::set_last_sent_timeslice(uint64_t sent_ts) {
  last_sent_timeslice_ = sent_ts;
}

void InputChannelConnection::ack_complete_interval_info() {
  const IntervalMetaData* meta_data =
      InputSchedulerOrchestrator::get_actual_meta_data(
          send_status_message_.actual_interval_metadata.interval_index !=
                  ConstVariables::MINUS_ONE
              ? send_status_message_.actual_interval_metadata.interval_index + 1
              : 0);
  if (meta_data != nullptr) {
    send_status_message_.actual_interval_metadata = *meta_data;
    send_status_message_.required_interval_index =
        meta_data->interval_index + 2;
    send_status_message_.median_latency = get_msg_median_latency();
    data_acked_ = true;
  }
}

uint64_t InputChannelConnection::get_msg_median_latency() {
  std::vector<uint64_t> temp_list(msg_latency_);
  std::sort(temp_list.begin(), temp_list.end());
  return temp_list[temp_list.size() / 2];
}

void InputChannelConnection::add_timeslice_data_address(uint64_t data_size,
                                                        uint64_t desc_size) {
  timeslice_data_address_.push_back(data_size);
  cn_wp_pending_.data += data_size;
  cn_wp_pending_.desc += desc_size;
}

void InputChannelConnection::on_complete_heartbeat_recv() {
  if (true) {
    L_(info) << "[c" << remote_index_ << "] "
             << "[" << index_ << "] "
             << "COMPLETE RECEIVE heartbeat message"
             << " (id=" << recv_heartbeat_message_.message_id
             << ", ACK=" << recv_heartbeat_message_.ack
             << ", FAILED=" << recv_heartbeat_message_.failure_info.index
             << ", DESC="
             << recv_heartbeat_message_.failure_info.last_completed_desc
             << ", TS="
             << recv_heartbeat_message_.failure_info.timeslice_trigger << ")";
  }
  if (done_)
    return;

  InputSchedulerOrchestrator::log_heartbeat(index_);
  if (InputSchedulerOrchestrator::is_connection_timed_out(index_)) {
    return;
  }

  // heartbeat message request for info about failed connection
  HeartbeatFailedNodeInfo* failed_conn = process_failed_connection_request();
  prepare_heartbeat_response(failed_conn);
  post_recv_heartbeat_message();
}

HeartbeatFailedNodeInfo*
InputChannelConnection::process_failed_connection_request() {
  HeartbeatFailedNodeInfo* failed_conn = nullptr;
  if (recv_heartbeat_message_.failure_info.index != ConstVariables::MINUS_ONE &&
      recv_heartbeat_message_.failure_info.timeslice_trigger ==
          ConstVariables::MINUS_ONE) {
    failed_conn = InputSchedulerOrchestrator::get_timed_out_connection(
        recv_heartbeat_message_.failure_info.index);
  }
  // Info to start re-distribution
  if (recv_heartbeat_message_.failure_info.index != ConstVariables::MINUS_ONE &&
      recv_heartbeat_message_.failure_info.last_completed_desc !=
          ConstVariables::MINUS_ONE &&
      recv_heartbeat_message_.failure_info.timeslice_trigger !=
          ConstVariables::MINUS_ONE) {
    InputSchedulerOrchestrator::consider_reschedule_decision(
        recv_heartbeat_message_.failure_info);
  }
  return failed_conn;
}

void InputChannelConnection::prepare_heartbeat_response(
    HeartbeatFailedNodeInfo* failed_conn) {
  if (!recv_heartbeat_message_.ack) {
    // TODO TO BE REMOVED ---- this is special condition to prevent
    // corrupted data
    if (recv_heartbeat_message_.failure_info.index !=
            ConstVariables::MINUS_ONE &&
        (recv_heartbeat_message_.failure_info.last_completed_desc !=
             ConstVariables::MINUS_ONE ||
         recv_heartbeat_message_.failure_info.timeslice_trigger !=
             ConstVariables::MINUS_ONE) &&
        (recv_heartbeat_message_.failure_info.last_completed_desc ==
             ConstVariables::MINUS_ONE ||
         recv_heartbeat_message_.failure_info.timeslice_trigger ==
             ConstVariables::MINUS_ONE)) {
      failed_conn = new HeartbeatFailedNodeInfo(
          recv_heartbeat_message_.failure_info.index);
      prepare_heartbeat(failed_conn,
                        SchedulerOrchestrator::get_next_heartbeat_message_id(),
                        false);
    } else {
      prepare_heartbeat(failed_conn, recv_heartbeat_message_.message_id, true);
    }
  } else
    SchedulerOrchestrator::acknowledge_heartbeat_message(
        recv_heartbeat_message_.message_id);
}

void InputChannelConnection::update_cn_wp_after_failure_action(
    uint32_t failed_connection_id) {
  std::pair<uint64_t, uint64_t> last_transmitted_timeslice_info =
      InputSchedulerOrchestrator::get_data_and_desc_of_last_timeslice(index_);
  std::pair<uint64_t, uint64_t> last_rdma_acked_timeslice_info =
      InputSchedulerOrchestrator::
          get_data_and_desc_of_last_rdma_acked_timeslice(index_);
  L_(info) << "[" << index_
           << "] update_cn_wp_after_failure_action last transmitted "
           << last_transmitted_timeslice_info.first << ","
           << last_transmitted_timeslice_info.second << " last rdma "
           << last_rdma_acked_timeslice_info.first << ","
           << last_rdma_acked_timeslice_info.second;
  if (last_rdma_acked_timeslice_info.first < cn_wp_.data) {
    L_(debug) << "[c" << remote_index_ << "] "
              << "[" << index_ << "] "
              << "update_cn_wp_after_failure_action"
              << " cn_wp_.data = " << cn_wp_.data
              << " cn_wp_.desc = " << cn_wp_.desc
              << " cn_wp_pending_.data = " << cn_wp_pending_.data
              << " cn_wp_pending_.desc = " << cn_wp_pending_.desc
              << " last_rdma_acked_timeslice_info.first = "
              << last_rdma_acked_timeslice_info.first
              << " last_rdma_acked_timeslice_info.second = "
              << last_rdma_acked_timeslice_info.second
              << " last_transmitted_timeslice_info.first = "
              << last_transmitted_timeslice_info.first
              << " last_transmitted_timeslice_info.second = "
              << last_transmitted_timeslice_info.second;
    cn_wp_.data = last_rdma_acked_timeslice_info.first;
    cn_wp_.desc = last_rdma_acked_timeslice_info.second;
    cn_wp_pending_.data = last_transmitted_timeslice_info.first - cn_wp_.data;
    cn_wp_pending_.desc = last_transmitted_timeslice_info.second - cn_wp_.desc;
    if (cn_ack_.data > cn_wp_.data) {
      cn_ack_.data = cn_wp_.data;
      cn_ack_.desc = cn_wp_.desc;
    }
    data_changed_ = true;
  }

  while (!pending_descriptors_.empty() &&
         pending_descriptors_.get_last_key() >
             last_transmitted_timeslice_info.second) {
    timeslice_data_address_.erase(--timeslice_data_address_.end());
    pending_descriptors_.remove(pending_descriptors_.get_last_key());
    data_acked_ = true;
  }
  sync_after_scheduling_decision_ = true;
  sync_failed_conn_ = failed_connection_id;
  try_sync_buffer_positions();
}

} // namespace tl_libfabric
