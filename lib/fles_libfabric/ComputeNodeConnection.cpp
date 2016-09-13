// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "ComputeNodeConnection.hpp"
#include "ComputeNodeInfo.hpp"
#include "RequestIdentifier.hpp"
#include "LibfabricException.hpp"
#include "Provider.hpp"
#include <cassert>
#include <log.hpp>

ComputeNodeConnection::ComputeNodeConnection(
                                             struct fid_eq *eq,
                                             uint_fast16_t connection_index,
                                             uint_fast16_t remote_connection_index,
    InputNodeInfo remote_info, uint8_t *data_ptr, uint32_t data_buffer_size_exp,
    fles::TimesliceComponentDescriptor *desc_ptr, uint32_t desc_buffer_size_exp)
  : Connection(eq, connection_index, remote_connection_index),
      remote_info_(std::move(remote_info)), data_ptr_(data_ptr),
      data_buffer_size_exp_(data_buffer_size_exp), desc_ptr_(desc_ptr),
      desc_buffer_size_exp_(desc_buffer_size_exp)
{
    // send and receive only single StatusMessage struct
    max_send_wr_ = 2; // one additional wr to avoid race (recv before
    // send completion)
    max_send_sge_ = 1;
    max_recv_wr_ = 1;
    max_recv_sge_ = 1;
}

void ComputeNodeConnection::post_recv_status_message()
{
    if (false) {
        L_(trace) << "[c" << remote_index_ << "] "
                  << "[" << index_ << "] "
                  << "POST RECEIVE status message";
    }
    post_recv_msg(&recv_wr);
}

void ComputeNodeConnection::post_send_status_message()
{
    if (false) {
        L_(trace) << "[c" << remote_index_ << "] "
                  << "[" << index_ << "] "
                  << "POST SEND status_message"
                  << " (ack.desc=" << send_status_message_.ack.desc << ")";
    }
    while (pending_send_requests_ >= 2000 /*qp_cap_.max_send_wr*/) {
        throw LibfabricException(
            "Max number of pending send requests exceeded");
    }
    ++pending_send_requests_;
    post_send_msg(&send_wr);
}

void ComputeNodeConnection::post_send_final_status_message()
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    send_wr.context = (void*)(ID_SEND_FINALIZE | (index_ << 8));
#pragma GCC diagnostic pop
    post_send_status_message();
}

void ComputeNodeConnection::setup_mr(struct fid_domain* pd)
{
    assert(data_ptr_ && desc_ptr_ && data_buffer_size_exp_ &&
           desc_buffer_size_exp_);

    int res;

    // register memory regions
    std::size_t data_bytes = UINT64_C(1) << data_buffer_size_exp_;
    std::size_t desc_bytes = (UINT64_C(1) << desc_buffer_size_exp_) *
                             sizeof(fles::TimesliceComponentDescriptor);
    res = fi_mr_reg(pd, data_ptr_, data_bytes, FI_WRITE | FI_REMOTE_WRITE, 0,
                    Provider::requested_key++, 0, &mr_data_, nullptr);
    if (res)
        throw LibfabricException("fi_mr_reg failed for data_ptr");
    res = fi_mr_reg(pd, desc_ptr_, desc_bytes, FI_WRITE | FI_REMOTE_WRITE, 0,
                    Provider::requested_key++, 0, &mr_desc_, nullptr);
    if (res)
        throw LibfabricException("fi_mr_reg failed for desc_ptr");
    res =
        fi_mr_reg(pd, &send_status_message_, sizeof(ComputeNodeStatusMessage),
                  FI_SEND, 0, Provider::requested_key++, 0, &mr_send_, nullptr);
    if (res)
        throw LibfabricException("fi_mr_reg failed for send");
    res =
        fi_mr_reg(pd, &recv_status_message_, sizeof(InputChannelStatusMessage),
                  FI_RECV, 0, Provider::requested_key++, 0, &mr_recv_, nullptr);
    if (res)
        throw LibfabricException("fi_mr_reg failed for recv");

    if (!mr_data_ || !mr_desc_ || !mr_recv_ || !mr_send_)
        throw LibfabricException(
            "registration of memory region failed in ComputeNodeConnection");
}

void ComputeNodeConnection::setup()
{
    // setup send and receive buffers
    recv_sge.iov_base = &recv_status_message_;
    recv_sge.iov_len = sizeof(InputChannelStatusMessage);

    recv_wr_descs[0] = fi_mr_desc(mr_recv_);

    recv_wr.msg_iov = &recv_sge;
    recv_wr.desc = recv_wr_descs;
    recv_wr.iov_count = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    recv_wr.context = (void *)(ID_RECEIVE_STATUS | (index_ << 8));
#pragma GCC diagnostic pop

    send_sge.iov_base = &send_status_message_;
    send_sge.iov_len = sizeof(ComputeNodeStatusMessage);

    send_wr_descs[0] = fi_mr_desc(mr_send_);

    send_wr.msg_iov = &send_sge;
    send_wr.desc = send_wr_descs;
    send_wr.iov_count = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    send_wr.context = (void *)(ID_SEND_STATUS | (index_ << 8));
#pragma GCC diagnostic pop

    // post initial receive request
    post_recv_status_message();
}

void ComputeNodeConnection::on_established(struct fi_eq_cm_entry *event)
{
    Connection::on_established(event);

    L_(debug) << "[c" << remote_index_ << "] "
              << "remote index: " << remote_info_.index;
}

void ComputeNodeConnection::on_disconnected(struct fi_eq_cm_entry *event)
{
    disconnect();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    if (mr_recv_) {
        fi_close((struct fid *)mr_recv_);
        mr_recv_ = nullptr;
    }

    if (mr_send_) {
        fi_close((struct fid *)mr_send_);
        mr_send_ = nullptr;
    }

    if (mr_desc_) {
        fi_close((struct fid *)mr_desc_);
        mr_desc_ = nullptr;
    }

    if (mr_data_) {
        fi_close((struct fid *)mr_data_);
        mr_data_ = nullptr;
    }
#pragma GCC diagnostic pop

    Connection::on_disconnected(event);
}

void ComputeNodeConnection::inc_ack_pointers(uint64_t ack_pos)
{
    cn_ack_.desc = ack_pos;

    const fles::TimesliceComponentDescriptor &acked_ts =
        desc_ptr_[(ack_pos - 1) & ((UINT64_C(1) << desc_buffer_size_exp_) - 1)];

    cn_ack_.data = acked_ts.offset + acked_ts.size;
}

void ComputeNodeConnection::on_complete_recv()
{
    if (recv_status_message_.final) {
        L_(debug) << "[c" << remote_index_ << "] "
                  << "[" << index_ << "] "
                  << "received FINAL status message";
        // send FINAL status message
        send_status_message_.final = true;
        post_send_final_status_message();
        return;
    }
    if (false) {
        L_(trace) << "[c" << remote_index_ << "] "
                  << "[" << index_ << "] "
                  << "COMPLETE RECEIVE status message"
                  << " (wp.desc=" << recv_status_message_.wp.desc << ")";
    }
    cn_wp_ = recv_status_message_.wp;
    post_recv_status_message();
    send_status_message_.ack = cn_ack_;
    post_send_status_message();
}

void ComputeNodeConnection::on_complete_send()
{
    pending_send_requests_--;
}

void ComputeNodeConnection::on_complete_send_finalize()
{
    done_ = true;
}

std::unique_ptr<std::vector<uint8_t> > ComputeNodeConnection::get_private_data()
{
    assert(data_ptr_ && desc_ptr_ && data_buffer_size_exp_ &&
           desc_buffer_size_exp_);
    std::unique_ptr<std::vector<uint8_t> > private_data(
        new std::vector<uint8_t>(sizeof(ComputeNodeInfo)));

    ComputeNodeInfo *cn_info =
        reinterpret_cast<ComputeNodeInfo *>(private_data->data());
    cn_info->data.addr = reinterpret_cast<uintptr_t>(data_ptr_);
    cn_info->data.rkey = fi_mr_key(mr_data_);
    cn_info->desc.addr = reinterpret_cast<uintptr_t>(desc_ptr_);
    cn_info->desc.rkey = fi_mr_key(mr_desc_);
    cn_info->index = remote_index_;
    cn_info->data_buffer_size_exp = data_buffer_size_exp_;
    cn_info->desc_buffer_size_exp = desc_buffer_size_exp_;

    return private_data;
}
