// Copyright 2012-2014 Jan de Cuveland <cmail@cuveland.de>

#include "InputChannelConnection.hpp"
#include "InputNodeInfo.hpp"
#include "MicrosliceDescriptor.hpp"
#include "RequestIdentifier.hpp"
#include "TimesliceComponentDescriptor.hpp"
#include "log.hpp"
#include <cassert>
#include <cstring>

InputChannelConnection::InputChannelConnection(
    struct rdma_event_channel* ec,
    uint_fast16_t connection_index,
    uint_fast16_t remote_connection_index,
    unsigned int max_send_wr,
    unsigned int max_pending_write_requests,
    struct rdma_cm_id* id)
    : IBConnection(ec, connection_index, remote_connection_index, id),
      max_pending_write_requests_(max_pending_write_requests) {
  assert(max_pending_write_requests_ > 0);

  qp_cap_.max_send_wr = max_send_wr; // typical hca maximum: 16k
  qp_cap_.max_send_sge = 4; // max. two chunks each for descriptors and data

  qp_cap_.max_recv_wr =
      1; // receive only single ComputeNodeStatusMessage struct
  qp_cap_.max_recv_sge = 1;

  qp_cap_.max_inline_data = sizeof(fles::TimesliceComponentDescriptor);
}

bool InputChannelConnection::check_for_buffer_space(uint64_t data_size,
                                                    uint64_t desc_size) {
  if (false) {
    L_(trace) << "[" << index_ << "] "
              << "SENDER data space (bytes) required=" << data_size
              << ", avail="
              << cn_ack_.data +
                     (UINT64_C(1) << remote_info_.data_buffer_size_exp) -
                     cn_wp_.data;
    L_(trace) << "[" << index_ << "] "
              << "SENDER desc space (entries) required=" << desc_size
              << ", avail="
              << cn_ack_.desc +
                     (UINT64_C(1) << remote_info_.desc_buffer_size_exp) -
                     cn_wp_.desc;
  }
  if (cn_ack_.data - cn_wp_.data +
              (UINT64_C(1) << remote_info_.data_buffer_size_exp) <
          data_size ||
      cn_ack_.desc - cn_wp_.desc +
              (UINT64_C(1) << remote_info_.desc_buffer_size_exp) <
          desc_size) { // TODO: extend condition!
    return false;
  }
  return true;
}

void InputChannelConnection::send_data(struct ibv_sge* sge,
                                       int num_sge,
                                       uint64_t timeslice,
                                       uint64_t desc_length,
                                       uint64_t data_length,
                                       uint64_t skip) {
  int num_sge2 = 0;
  struct ibv_sge sge2[4];

  uint64_t cn_wp_data = cn_wp_.data;
  cn_wp_data += skip;

  uint64_t cn_data_buffer_mask =
      (UINT64_C(1) << remote_info_.data_buffer_size_exp) - 1;
  uint64_t cn_desc_buffer_mask =
      (UINT64_C(1) << remote_info_.desc_buffer_size_exp) - 1;
  uint64_t target_bytes_left =
      (UINT64_C(1) << remote_info_.data_buffer_size_exp) -
      (cn_wp_data & cn_data_buffer_mask);

  // split sge list if necessary
  int num_sge_cut = 0;
  if (data_length + desc_length * sizeof(fles::MicrosliceDescriptor) >
      target_bytes_left) {
    for (int i = 0; i < num_sge; ++i) {
      if (sge[i].length <= target_bytes_left) {
        target_bytes_left -= sge[i].length;
      } else {
        if (target_bytes_left != 0u) {
          sge2[num_sge2].addr = sge[i].addr + target_bytes_left;
          sge2[num_sge2].length = sge[i].length - target_bytes_left;
          sge2[num_sge2++].lkey = sge[i].lkey;
          sge[i].length = target_bytes_left;
          target_bytes_left = 0;
        } else {
          sge2[num_sge2++] = sge[i];
          ++num_sge_cut;
        }
      }
    }
  }
  num_sge -= num_sge_cut;

  struct ibv_send_wr send_wr_ts;
  struct ibv_send_wr send_wr_tswrap;
  struct ibv_send_wr send_wr_tscdesc;
  memset(&send_wr_ts, 0, sizeof(send_wr_ts));
  send_wr_ts.wr_id = ID_WRITE_DATA;
  send_wr_ts.opcode = IBV_WR_RDMA_WRITE;
  send_wr_ts.sg_list = sge;
  send_wr_ts.num_sge = num_sge;
  send_wr_ts.wr.rdma.rkey = remote_info_.data.rkey;
  send_wr_ts.wr.rdma.remote_addr = static_cast<uintptr_t>(
      remote_info_.data.addr + (cn_wp_data & cn_data_buffer_mask));

  if (num_sge2 != 0) {
    memset(&send_wr_tswrap, 0, sizeof(send_wr_ts));
    send_wr_tswrap.wr_id = ID_WRITE_DATA_WRAP;
    send_wr_tswrap.opcode = IBV_WR_RDMA_WRITE;
    send_wr_tswrap.sg_list = sge2;
    send_wr_tswrap.num_sge = num_sge2;
    send_wr_tswrap.wr.rdma.rkey = remote_info_.data.rkey;
    send_wr_tswrap.wr.rdma.remote_addr =
        static_cast<uintptr_t>(remote_info_.data.addr);
    send_wr_ts.next = &send_wr_tswrap;
    send_wr_tswrap.next = &send_wr_tscdesc;
  } else {
    send_wr_ts.next = &send_wr_tscdesc;
  }

  // timeslice component descriptor
  fles::TimesliceComponentDescriptor tscdesc;
  tscdesc.ts_num = timeslice;
  tscdesc.offset = cn_wp_data;
  tscdesc.size = data_length + desc_length * sizeof(fles::MicrosliceDescriptor);
  tscdesc.num_microslices = desc_length;
  struct ibv_sge sge3;
  sge3.addr = reinterpret_cast<uintptr_t>(&tscdesc);
  sge3.length = sizeof(tscdesc);
  sge3.lkey = 0;

  memset(&send_wr_tscdesc, 0, sizeof(send_wr_tscdesc));
  send_wr_tscdesc.wr_id = ID_WRITE_DESC | (timeslice << 24) | (index_ << 8);
  send_wr_tscdesc.opcode = IBV_WR_RDMA_WRITE;
  send_wr_tscdesc.send_flags =
      IBV_SEND_INLINE | IBV_SEND_FENCE | IBV_SEND_SIGNALED;
  send_wr_tscdesc.sg_list = &sge3;
  send_wr_tscdesc.num_sge = 1;
  send_wr_tscdesc.wr.rdma.rkey = remote_info_.desc.rkey;
  send_wr_tscdesc.wr.rdma.remote_addr = static_cast<uintptr_t>(
      remote_info_.desc.addr + (cn_wp_.desc & cn_desc_buffer_mask) *
                                   sizeof(fles::TimesliceComponentDescriptor));

  if (false) {
    L_(trace) << "[i" << remote_index_ << "] "
              << "[" << index_ << "] "
              << "POST SEND data (timeslice " << timeslice << ")";
  }

  // send everything
  assert(pending_write_requests_ < max_pending_write_requests_);
  ++pending_write_requests_;
  post_send(&send_wr_ts);
}

bool InputChannelConnection::write_request_available() {
  return (pending_write_requests_ < max_pending_write_requests_);
}

void InputChannelConnection::inc_write_pointers(uint64_t data_size,
                                                uint64_t desc_size) {
  cn_wp_.data += data_size;
  cn_wp_.desc += desc_size;
}

bool InputChannelConnection::try_sync_buffer_positions() {
  if (our_turn_) {
    our_turn_ = false;
    send_status_message_.wp = cn_wp_;
    post_send_status_message();
    return true;
  }
  return false;
}

uint64_t InputChannelConnection::skip_required(uint64_t data_size) {
  uint64_t databuf_size = UINT64_C(1) << remote_info_.data_buffer_size_exp;
  uint64_t databuf_wp = cn_wp_.data & (databuf_size - 1);
  if (databuf_wp + data_size <= databuf_size) {
    return 0;
  }
  return databuf_size - databuf_wp;
}

void InputChannelConnection::finalize(bool abort) {
  finalize_ = true;
  abort_ = abort;
  if (our_turn_) {
    our_turn_ = false;
    if (cn_wp_ == cn_ack_ || abort_) {
      send_status_message_.final = true;
      send_status_message_.abort = abort_;
    } else {
      send_status_message_.wp = cn_wp_;
    }
    post_send_status_message();
  }
}

void InputChannelConnection::on_complete_write() { pending_write_requests_--; }

void InputChannelConnection::on_complete_recv() {
  if (recv_status_message_.final) {
    done_ = true;
    return;
  }
  if (false) {
    L_(trace) << "[i" << remote_index_ << "] "
              << "[" << index_ << "] "
              << "receive completion, new cn_ack_.data="
              << recv_status_message_.ack.data;
  }
  cn_ack_ = recv_status_message_.ack;
  post_recv_status_message();

  if (cn_wp_ == send_status_message_.wp && finalize_) {
    if (cn_wp_ == cn_ack_ || abort_) {
      send_status_message_.final = true;
      send_status_message_.abort = abort_;
    }
    post_send_status_message();
  } else {
    our_turn_ = true;
  }
}

void InputChannelConnection::setup(struct ibv_pd* pd) {
  // register memory regions
  mr_recv_ =
      ibv_reg_mr(pd, &recv_status_message_, sizeof(ComputeNodeStatusMessage),
                 IBV_ACCESS_LOCAL_WRITE);
  if (mr_recv_ == nullptr) {
    throw InfinibandException("registration of memory region failed");
  }

  mr_send_ = ibv_reg_mr(pd, &send_status_message_,
                        sizeof(InputChannelStatusMessage), 0);
  if (mr_send_ == nullptr) {
    throw InfinibandException("registration of memory region failed");
  }

  // setup send and receive buffers
  recv_sge.addr = reinterpret_cast<uintptr_t>(&recv_status_message_);
  recv_sge.length = sizeof(ComputeNodeStatusMessage);
  recv_sge.lkey = mr_recv_->lkey;

  recv_wr.wr_id = ID_RECEIVE_STATUS | (index_ << 8);
  recv_wr.sg_list = &recv_sge;
  recv_wr.num_sge = 1;

  send_sge.addr = reinterpret_cast<uintptr_t>(&send_status_message_);
  send_sge.length = sizeof(InputChannelStatusMessage);
  send_sge.lkey = mr_send_->lkey;

  send_wr.wr_id = ID_SEND_STATUS | (index_ << 8);
  send_wr.opcode = IBV_WR_SEND;
  send_wr.send_flags = IBV_SEND_SIGNALED;
  send_wr.sg_list = &send_sge;
  send_wr.num_sge = 1;

  // post initial receive request
  post_recv_status_message();
}

/// Connection handler function, called on successful connection.
/**
   \param event RDMA connection manager event structure
*/
void InputChannelConnection::on_established(struct rdma_cm_event* event) {
  assert(event->param.conn.private_data_len >= sizeof(ComputeNodeInfo));
  memcpy(&remote_info_, event->param.conn.private_data,
         sizeof(ComputeNodeInfo));

  IBConnection::on_established(event);
}

void InputChannelConnection::dereg_mr() {
  if (mr_recv_ != nullptr) {
    ibv_dereg_mr(mr_recv_);
    mr_recv_ = nullptr;
  }

  if (mr_send_ != nullptr) {
    ibv_dereg_mr(mr_send_);
    mr_send_ = nullptr;
  }
}

void InputChannelConnection::on_rejected(struct rdma_cm_event* event) {
  dereg_mr();
  IBConnection::on_rejected(event);
}

void InputChannelConnection::on_disconnected(struct rdma_cm_event* event) {
  dereg_mr();
  IBConnection::on_disconnected(event);
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
  post_recv(&recv_wr);
}

void InputChannelConnection::post_send_status_message() {
  if (false) {
    L_(trace) << "[i" << remote_index_ << "] "
              << "[" << index_ << "] "
              << "POST SEND status message (wp.data="
              << send_status_message_.wp.data
              << " wp.desc=" << send_status_message_.wp.desc << ")";
  }
  post_send(&send_wr);
}
