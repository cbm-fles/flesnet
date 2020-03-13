// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ComputeNodeInfo.hpp"
#include "ComputeNodeStatusMessage.hpp"
#include "IBConnection.hpp"
#include "InputChannelStatusMessage.hpp"

/// Input node connection class.
/** An InputChannelConnection object represents the endpoint of a single
    timeslice building connection from an input node to a compute
    node. */

class InputChannelConnection : public IBConnection {
public:
  /// The InputChannelConnection constructor.
  InputChannelConnection(struct rdma_event_channel* ec,
                         uint_fast16_t connection_index,
                         uint_fast16_t remote_connection_index,
                         unsigned int max_send_wr,
                         unsigned int max_pending_write_requests,
                         struct rdma_cm_id* id = nullptr);

  InputChannelConnection(const InputChannelConnection&) = delete;
  void operator=(const InputChannelConnection&) = delete;

  /// Wait until enough space is available at target compute node.
  bool check_for_buffer_space(uint64_t data_size, uint64_t desc_size);

  /// Send data and descriptors to compute node.
  void send_data(struct ibv_sge* sge,
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

  /// Handle Infiniband receive completion notification.
  void on_complete_recv();

  void setup(struct ibv_pd* pd) override;

  /// Connection handler function, called on successful connection.
  /**
     \param event RDMA connection manager event structure
  */
  void on_established(struct rdma_cm_event* event) override;

  void dereg_mr();

  void on_rejected(struct rdma_cm_event* event) override;

  void on_disconnected(struct rdma_cm_event* event) override;

  std::unique_ptr<std::vector<uint8_t>> get_private_data() override;

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

  /// Infiniband memory region descriptor for CN status (including
  /// acknowledged-by-CN pointers)
  ibv_mr* mr_recv_ = nullptr;

  /// Local version of CN write pointers
  ComputeNodeBufferPosition cn_wp_ = ComputeNodeBufferPosition();

  /// Send buffer for input channel status (including CN write pointers)
  InputChannelStatusMessage send_status_message_ = InputChannelStatusMessage();

  /// Infiniband memory region descriptor for input channel status (including
  /// CN write pointers)
  ibv_mr* mr_send_ = nullptr;

  /// InfiniBand receive work request
  ibv_recv_wr recv_wr = ibv_recv_wr();

  /// Scatter/gather list entry for receive work request
  ibv_sge recv_sge = ibv_sge();

  /// Infiniband send work request
  ibv_send_wr send_wr = ibv_send_wr();

  /// Scatter/gather list entry for send work request
  ibv_sge send_sge = ibv_sge();

  unsigned int pending_write_requests_{0};

  unsigned int max_pending_write_requests_{0};
};
