// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include "ConstVariables.hpp"
#include "RequestIdentifier.hpp"
#include "SizedMap.hpp"
#include "dfs/HeartbeatMessage.hpp"
#include "dfs/SchedulerOrchestrator.hpp"
#include "providers/LibfabricBarrier.hpp"
#include "providers/LibfabricContextPool.hpp"
#include "providers/LibfabricException.hpp"
#include "providers/Provider.hpp"

#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <log.hpp>
#include <memory>
#include <netdb.h>
#include <thread>
#include <vector>

#include <sys/uio.h>

namespace tl_libfabric {
/// Libfabric connection base class.
/** An Connection object represents the endpoint of a single
    Libfabric connection handled by an rdma connection manager. */

class Connection {
public:
  /// The Connection constructor. Creates an endpoint.
  Connection(struct fid_eq* eq,
             uint_fast16_t connection_index,
             uint_fast16_t remote_connection_index);

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  /// The Connection destructor.
  virtual ~Connection();

  /// Initiate a connection request to target hostname and service.
  /**
     \param hostname The target hostname
     \param service  The target service or port number
  */
  void connect(const std::string& hostname,
               const std::string& service,
               struct fid_domain* domain,
               struct fid_cq* cq,
               struct fid_av* av);

  void disconnect();

  virtual void on_rejected(struct fi_eq_err_entry* event);

  /// Connection handler function, called on successful connection.
  /**
     \param event RDMA connection manager event structure
     \return      Non-zero if an error occured
  */
  virtual void on_established(struct fi_eq_cm_entry* event);

  /// Handle RDMA_CM_EVENT_DISCONNECTED event for this connection.
  virtual void on_disconnected(struct fi_eq_cm_entry* event);

  /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event for this connection.
  virtual void on_connect_request(struct fi_eq_cm_entry* event,
                                  struct fid_domain* pd,
                                  struct fid_cq* cq);

  virtual std::unique_ptr<std::vector<uint8_t>> get_private_data();

  virtual void setup() = 0;

  virtual void setup_mr(struct fid_domain* pd) = 0;

  virtual bool try_sync_buffer_positions() = 0;

  /// Handle Libfabric receive completion notification.
  virtual void on_complete_recv() = 0;

  /// Handle Libfabric send completion notification.
  virtual void on_complete_send() = 0;

  /// Handle Libfabric receive hearbeat completion notification.
  virtual void on_complete_heartbeat_recv() = 0;

  /// Handle Libfabric send hearbeat completion notification.
  void on_complete_heartbeat_send();

  /// Post a receive work request (WR) to the receive queue
  virtual void post_recv_heartbeat_message();

  /// Post a send work request (WR) to the send queue
  virtual void post_send_heartbeat_message();

  /// Retrieve index of this connection in the local connection group.
  uint_fast16_t index() const { return index_; }

  /// Retrieve index of this connection in the remote connection group.
  uint_fast16_t remote_index() const { return remote_index_; }

  bool done() const { return done_; }

  void mark_done() { done_ = true; }

  /// Retrieve the total number of bytes transmitted.
  uint64_t total_bytes_sent() const { return total_bytes_sent_; }

  /// Retrieve the total number of syn msgs bytes transmitted.
  uint64_t total_sync_bytes_sent() const { return total_sync_bytes_sent_; }

  /// Retrieve the total number of SEND work requests.
  uint64_t total_send_requests() const { return total_send_requests_; }

  /// Retrieve the total number of RECV work requests.
  uint64_t total_recv_requests() const { return total_recv_requests_; }

  /// Prepare heartbeat message
  void prepare_heartbeat(HeartbeatFailedNodeInfo* failure_info = nullptr,
                         uint64_t message_id = ConstVariables::MINUS_ONE,
                         bool ack = false);

  void send_heartbeat(HeartbeatMessage* message);

  /// Get the last state of the send_heartbeat_message
  const HeartbeatMessage get_send_heartbeat_message() {
    return send_heartbeat_message_;
  }

  /// Get the last state of the send_heartbeat_message
  const HeartbeatMessage get_recv_heartbeat_message() {
    return recv_heartbeat_message_;
  }

  std::chrono::high_resolution_clock::time_point time_begin_;

protected:
  /// Post an Libfabric rdma send work request
  bool post_send_rdma(struct fi_msg_rma* wr, uint64_t flags);

  /// Post an Libfabric message send work request
  bool post_send_msg(const struct fi_msg_tagged* wr);

  /// Post an Libfabric message recveive request.
  bool post_recv_msg(const struct fi_msg_tagged* wr);

  void make_endpoint(struct fi_info* info,
                     const std::string& hostname,
                     const std::string& service,
                     struct fid_domain* pd,
                     struct fid_cq* cq,
                     struct fid_av* av);

  /// Message setup of heartbeat messages
  virtual void setup_heartbeat();

  /// Memory registers setup of heartbeat messages
  virtual void setup_heartbeat_mr(struct fid_domain* pd);

  /// Index of this connection in the local group of connections.
  uint_fast16_t index_;

  /// Index of this connection in the remote group of connections.
  uint_fast16_t remote_index_;

  /// Flag indicating connection finished state.
  bool done_ = false;

  /// connection configuration
  uint32_t max_send_wr_;
  uint32_t max_send_sge_;
  uint32_t max_recv_wr_;
  uint32_t max_recv_sge_;
  uint32_t max_inline_data_;

  struct fid_ep* ep_ = nullptr;

  bool connection_oriented_ = false;

  /// check if new data should be sent
  bool data_changed_ = false;

  /// check if new data is acked and should be sent
  bool data_acked_ = false;

  /// To prevent reusing the buffer while injecting sync messages
  bool send_buffer_available_ = true;

  /// To prevent reusing the buffer while injecting heartbeat messages
  bool heartbeat_send_buffer_available_ = true;

  /// Send Heartbeat message buffer
  HeartbeatMessage send_heartbeat_message_ = HeartbeatMessage();

  /// Receive Heartbeat message buffer
  HeartbeatMessage recv_heartbeat_message_ = HeartbeatMessage();

  /// heartbeat recv work request
  struct fi_msg_tagged heartbeat_recv_wr = fi_msg_tagged();
  struct iovec heartbeat_recv_wr_iovec = iovec();
  void* heartbeat_recv_descs[1] = {nullptr};
  fid_mr* mr_heartbeat_recv_ = nullptr;

  /// heartbeat send work request
  struct fi_msg_tagged heartbeat_send_wr = fi_msg_tagged();
  struct iovec heartbeat_send_wr_iovec = iovec();
  void* heartbeat_send_descs[1] = {nullptr};
  fid_mr* mr_heartbeat_send_ = nullptr;

private:
  /// event queue
  struct fid_eq* eq_ = nullptr;

  /// Total number of bytes transmitted.
  uint64_t total_bytes_sent_ = 0;

  /// Total number of sync messages bytes transmitted.
  uint64_t total_sync_bytes_sent_ = 0;

  /// Total number of SEND work requests.
  uint64_t total_send_requests_ = 0;

  /// Total number of RECV work requests.
  uint64_t total_recv_requests_ = 0;
};
} // namespace tl_libfabric
