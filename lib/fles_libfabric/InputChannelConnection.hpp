// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include "ComputeNodeInfo.hpp"
#include "ComputeNodeStatusMessage.hpp"
#include "Connection.hpp"
#include "InputChannelStatusMessage.hpp"

#include <sys/uio.h>

namespace tl_libfabric {
/// Input node connection class.
/** An InputChannelConnection object represents the endpoint of a single
 timeslice building connection from an input node to a compute
 node. */

class InputChannelConnection : public Connection {
public:
  /// The InputChannelConnection constructor.
  InputChannelConnection(struct fid_eq* eq,
                         uint_fast16_t connection_index,
                         uint_fast16_t remote_connection_index,
                         unsigned int max_send_wr,
                         unsigned int max_pending_write_requests);

  InputChannelConnection(const InputChannelConnection&) = delete;
  void operator=(const InputChannelConnection&) = delete;

  /// Wait until enough space is available at target compute node.
  bool check_for_buffer_space(uint64_t data_size, uint64_t desc_size);

  /// Send data and descriptors to compute node.
  void send_data(struct iovec* sge,
                 void** desc,
                 int num_sge,
                 uint64_t timeslice,
                 uint64_t desc_length,
                 uint64_t data_length,
                 uint64_t skip);

  bool write_request_available();

  /// Increment target write pointers after data has been sent.
  void inc_write_pointers(uint64_t data_size, uint64_t desc_size);

  // Get number of bytes to skip in advance (to avoid buffer wrap)
  uint64_t skip_required(uint64_t data_size);

  bool try_sync_buffer_positions();

  void finalize(bool abort);

  bool request_abort_flag() { return recv_status_message_.request_abort; }

  void on_complete_write();

  /// Handle Libfabric receive completion notification.
  void on_complete_recv();

  void setup_mr(struct fid_domain* pd) override;
  void setup() override;

  /// Connection handler function, called on successful connection.
  /**
   \param event RDMA connection manager event structure
   */
  void on_established(struct fi_eq_cm_entry* event) override;
  //
  void dereg_mr();

  void on_rejected(struct fi_eq_err_entry* event) override;

  void on_disconnected(struct fi_eq_cm_entry* event) override;

  std::unique_ptr<std::vector<uint8_t>> get_private_data() override;

  void connect(const std::string& hostname,
               const std::string& service,
               struct fid_domain* domain,
               struct fid_cq* cq,
               struct fid_av* av,
               fi_addr_t fi_addr);

  void reconnect();

  void set_partner_addr(struct fid_av* av_);

  fi_addr_t get_partner_addr();

  void set_remote_info();

private:
  /// Post a receive work request (WR) to the receive queue
  void post_recv_status_message();

  /// Post a send work request (WR) to the send queue
  void post_send_status_message();

  /// Flag, true if it is the input nodes's turn to send a pointer update.
  bool our_turn_ = true;

  bool finalize_ = false;
  bool abort_ = false;

  /// Access information for memory regions on remote end.
  ComputeNodeInfo remote_info_ = ComputeNodeInfo();

  /// Local copy of acknowledged-by-CN pointers
  ComputeNodeBufferPosition cn_ack_ = ComputeNodeBufferPosition();

  /// Receive buffer for CN status (including acknowledged-by-CN pointers)
  ComputeNodeStatusMessage recv_status_message_ = ComputeNodeStatusMessage();

  /// Libfabric memory region descriptor for CN status (including
  /// acknowledged-by-CN pointers)
  fid_mr* mr_recv_ = nullptr;

  /// Local version of CN write pointers
  ComputeNodeBufferPosition cn_wp_ = ComputeNodeBufferPosition();

  /// Send buffer for input channel status (including CN write pointers)
  InputChannelStatusMessage send_status_message_ = InputChannelStatusMessage();

  /// Libfabric memory region descriptor for input channel status (including
  /// CN write pointers)
  struct fid_mr* mr_send_ = nullptr;

  /// InfiniBand receive work request
  struct fi_msg recv_wr = fi_msg();
  struct iovec recv_wr_iovec = iovec();
  void* recv_descs[1] = {nullptr};

  /// Infiniband send work request
  struct fi_msg send_wr = fi_msg();
  struct iovec send_wr_iovec = iovec();
  void* send_descs[1] = {nullptr};

  unsigned int pending_write_requests_{0};

  unsigned int max_pending_write_requests_{0};

  fi_addr_t partner_addr_ = 0;
};
} // namespace tl_libfabric
