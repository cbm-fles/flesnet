// Copyright 2012-2025 Jan de Cuveland <cmail@cuveland.de>

#include "ComputeNodeConnectionUcx.hpp"
#include "ComputeNodeInfoUcx.hpp"
#include "RequestIdentifierUcx.hpp"
#include "log.hpp"
#include <cassert>

#define WITH_TRACE 0

ComputeNodeConnectionUcx::ComputeNodeConnectionUcx(
    struct rdma_event_channel* ec,
    uint_fast16_t connection_index,
    uint_fast16_t remote_connection_index,
    struct rdma_cm_id* id,
    InputNodeInfoUcx remote_info,
    uint8_t* data_ptr,
    uint32_t data_buffer_size_exp,
    fles::TimesliceComponentDescriptor* desc_ptr,
    uint32_t desc_buffer_size_exp)
    : IBConnectionUcx(ec, connection_index, remote_connection_index, id),
      remote_info_(remote_info), data_ptr_(data_ptr),
      data_buffer_size_exp_(data_buffer_size_exp), desc_ptr_(desc_ptr),
      desc_buffer_size_exp_(desc_buffer_size_exp) {
  // send and receive only single StatusMessage struct
  qp_cap_.max_send_wr = 2; // one additional wr to avoid race (recv before
  // send completion)
  qp_cap_.max_send_sge = 1;
  qp_cap_.max_recv_wr = 1;
  qp_cap_.max_recv_sge = 1;
}

void ComputeNodeConnectionUcx::post_recv_status_message() {
#if WITH_TRACE
  L_(trace) << "[c" << remote_index_ << "] " << "[" << index_ << "] "
            << "POST RECEIVE status message";
#endif
  post_recv(&recv_wr);
}

void ComputeNodeConnectionUcx::post_send_status_message() {
#if WITH_TRACE
  L_(trace) << "[c" << remote_index_ << "] " << "[" << index_ << "] "
            << "POST SEND status_message"
            << " (ack.desc=" << send_status_message_.ack.desc << ")";
#endif
  while (pending_send_requests_ >= qp_cap_.max_send_wr) {
    throw InfinibandExceptionUcx(
        "Max number of pending send requests exceeded");
  }
  ++pending_send_requests_;
  post_send(&send_wr);
}

void ComputeNodeConnectionUcx::post_send_final_status_message() {
  send_wr.wr_id = ID_SEND_FINALIZE | (index_ << 8);
  send_wr.send_flags = IBV_SEND_SIGNALED;
  post_send_status_message();
}

void ComputeNodeConnectionUcx::setup(struct ibv_pd* pd) {
  assert(data_ptr_ && desc_ptr_ && data_buffer_size_exp_ &&
         desc_buffer_size_exp_);

  // register memory regions
  std::size_t data_bytes = UINT64_C(1) << data_buffer_size_exp_;
  std::size_t desc_bytes = (UINT64_C(1) << desc_buffer_size_exp_) *
                           sizeof(fles::TimesliceComponentDescriptor);
  mr_data_ = ibv_reg_mr(pd, data_ptr_, data_bytes,
                        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  mr_desc_ = ibv_reg_mr(pd, desc_ptr_, desc_bytes,
                        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  mr_send_ = ibv_reg_mr(pd, &send_status_message_,
                        sizeof(ComputeNodeStatusMessageUcx), 0);
  mr_recv_ =
      ibv_reg_mr(pd, &recv_status_message_,
                 sizeof(InputChannelStatusMessageUcx), IBV_ACCESS_LOCAL_WRITE);

  if ((mr_data_ == nullptr) || (mr_desc_ == nullptr) || (mr_recv_ == nullptr) ||
      (mr_send_ == nullptr)) {
    throw InfinibandExceptionUcx("registration of memory region failed");
  }

  // setup send and receive buffers
  recv_sge.addr = reinterpret_cast<uintptr_t>(&recv_status_message_);
  recv_sge.length = sizeof(InputChannelStatusMessageUcx);
  recv_sge.lkey = mr_recv_->lkey;

  recv_wr.wr_id = ID_RECEIVE_STATUS | (index_ << 8);
  recv_wr.sg_list = &recv_sge;
  recv_wr.num_sge = 1;

  send_sge.addr = reinterpret_cast<uintptr_t>(&send_status_message_);
  send_sge.length = sizeof(ComputeNodeStatusMessageUcx);
  send_sge.lkey = mr_send_->lkey;

  send_wr.wr_id = ID_SEND_STATUS | (index_ << 8);
  send_wr.opcode = IBV_WR_SEND;
  send_wr.send_flags = IBV_SEND_SIGNALED;
  send_wr.sg_list = &send_sge;
  send_wr.num_sge = 1;

  // post initial receive request
  post_recv_status_message();
}

void ComputeNodeConnectionUcx::on_established(struct rdma_cm_event* event) {
  IBConnectionUcx::on_established(event);

  L_(debug) << "[c" << remote_index_ << "] "
            << "remote index: " << remote_info_.index;
}

void ComputeNodeConnectionUcx::on_disconnected(struct rdma_cm_event* event) {
  disconnect();

  if (mr_recv_ != nullptr) {
    ibv_dereg_mr(mr_recv_);
    mr_recv_ = nullptr;
  }

  if (mr_send_ != nullptr) {
    ibv_dereg_mr(mr_send_);
    mr_send_ = nullptr;
  }

  if (mr_desc_ != nullptr) {
    ibv_dereg_mr(mr_desc_);
    mr_desc_ = nullptr;
  }

  if (mr_data_ != nullptr) {
    ibv_dereg_mr(mr_data_);
    mr_data_ = nullptr;
  }

  IBConnectionUcx::on_disconnected(event);
}

void ComputeNodeConnectionUcx::inc_ack_pointers(uint64_t ack_pos) {
  cn_ack_.desc = ack_pos;

  const fles::TimesliceComponentDescriptor& acked_ts =
      desc_ptr_[(ack_pos - 1) & ((UINT64_C(1) << desc_buffer_size_exp_) - 1)];

  cn_ack_.data = acked_ts.offset + acked_ts.size;
}

void ComputeNodeConnectionUcx::on_complete_recv() {
  if (recv_status_message_.final) {
    L_(debug) << "[c" << remote_index_ << "] " << "[" << index_ << "] "
              << "received FINAL status message";
    // send FINAL status message
    send_status_message_.final = true;
    post_send_final_status_message();
    return;
  }
#if WITH_TRACE
  L_(trace) << "[c" << remote_index_ << "] " << "[" << index_ << "] "
            << "COMPLETE RECEIVE status message"
            << " (wp.desc=" << recv_status_message_.wp.desc << ")";
#endif
  cn_wp_ = recv_status_message_.wp;
  post_recv_status_message();
  send_status_message_.ack = cn_ack_;
  post_send_status_message();
}

void ComputeNodeConnectionUcx::on_complete_send() { pending_send_requests_--; }

void ComputeNodeConnectionUcx::on_complete_send_finalize() { done_ = true; }

std::unique_ptr<std::vector<uint8_t>>
ComputeNodeConnectionUcx::get_private_data() {
  assert(data_ptr_ && desc_ptr_ && data_buffer_size_exp_ &&
         desc_buffer_size_exp_);
  std::unique_ptr<std::vector<uint8_t>> private_data(
      new std::vector<uint8_t>(sizeof(ComputeNodeInfoUcx)));

  auto* cn_info = reinterpret_cast<ComputeNodeInfoUcx*>(private_data->data());
  cn_info->data.addr = reinterpret_cast<uintptr_t>(data_ptr_);
  cn_info->data.rkey = mr_data_->rkey;
  cn_info->desc.addr = reinterpret_cast<uintptr_t>(desc_ptr_);
  cn_info->desc.rkey = mr_desc_->rkey;
  cn_info->index = remote_index_;
  cn_info->data_buffer_size_exp = data_buffer_size_exp_;
  cn_info->desc_buffer_size_exp = desc_buffer_size_exp_;

  return private_data;
}
