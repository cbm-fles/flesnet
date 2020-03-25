// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "InfinibandException.hpp"
#include <memory>
#include <rdma/rdma_cma.h>
#include <vector>

/// InfiniBand connection base class.
/** An IBConnection object represents the endpoint of a single
    InfiniBand connection handled by an rdma connection manager. */

class IBConnection {
public:
  /// The IBConnection constructor. Creates a connection manager ID.
  IBConnection(struct rdma_event_channel* ec,
               uint_fast16_t connection_index,
               uint_fast16_t remote_connection_index,
               struct rdma_cm_id* id = nullptr);

  IBConnection(const IBConnection&) = delete;
  IBConnection& operator=(const IBConnection&) = delete;

  /// The IBConnection destructor.
  virtual ~IBConnection();

  /// Retrieve the InfiniBand queue pair associated with the connection.
  struct ibv_qp* qp() const {
    return cm_id_->qp;
  }

  /// Initiate a connection request to target hostname and service.
  /**
     \param hostname The target hostname
     \param service  The target service or port number
  */
  void connect(const std::string& hostname, const std::string& service);

  void disconnect();

  virtual void on_rejected(struct rdma_cm_event* event);

  /// Connection handler function, called on successful connection.
  /**
     \param event RDMA connection manager event structure
     \return      Non-zero if an error occured
  */
  virtual void on_established(struct rdma_cm_event* event);

  /// Handle RDMA_CM_EVENT_DISCONNECTED event for this connection.
  virtual void on_disconnected(struct rdma_cm_event* event);

  /// Handle RDMA_CM_EVENT_TIMEWAIT_EXIT event for this connection.
  virtual void on_timewait_exit(struct rdma_cm_event* event);

  /// Handle RDMA_CM_EVENT_ADDR_RESOLVED event for this connection.
  virtual void on_addr_resolved(struct ibv_pd* pd, struct ibv_cq* cq);

  virtual void create_qp(struct ibv_pd* pd, struct ibv_cq* cq);

  virtual void accept_connect_request();

  /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event for this connection.
  virtual void on_connect_request(struct rdma_cm_event* event,
                                  struct ibv_pd* pd,
                                  struct ibv_cq* cq);

  virtual std::unique_ptr<std::vector<uint8_t>> get_private_data();

  virtual void setup(struct ibv_pd* pd) = 0;

  /// Handle RDMA_CM_EVENT_ROUTE_RESOLVED event for this connection.
  virtual void on_route_resolved();

  /// Retrieve index of this connection in the local connection group.
  uint_fast16_t index() const { return index_; }

  /// Retrieve index of this connection in the remote connection group.
  uint_fast16_t remote_index() const { return remote_index_; }

  bool done() const { return done_; }

  /// Retrieve the total number of bytes transmitted.
  uint64_t total_bytes_sent() const { return total_bytes_sent_; }

  /// Retrieve the total number of SEND work requests.
  uint64_t total_send_requests() const { return total_send_requests_; }

  /// Retrieve the total number of RECV work requests.
  uint64_t total_recv_requests() const { return total_recv_requests_; }

protected:
  static void dump_send_wr(struct ibv_send_wr* wr);

  /// Post an InfiniBand SEND work request (WR) to the send queue
  void post_send(struct ibv_send_wr* wr);

  /// Post an InfiniBand RECV work request (WR) to the receive queue.
  void post_recv(struct ibv_recv_wr* wr);

  /// Index of this connection in the local group of connections.
  uint_fast16_t index_;

  /// Index of this connection in the remote group of connections.
  uint_fast16_t remote_index_;

  /// Flag indicating connection finished state.
  bool done_ = false;

  /// The queue pair capabilities.
  struct ibv_qp_cap qp_cap_;

private:
  /// Low-level communication parameters.
  enum {
    RESOLVE_TIMEOUT_MS = 5000 ///< Resolve timeout in milliseconds.
  };

  /// RDMA connection manager ID.
  struct rdma_cm_id* cm_id_ = nullptr;

  /// Total number of bytes transmitted.
  uint64_t total_bytes_sent_ = 0;

  /// Total number of SEND work requests.
  uint64_t total_send_requests_ = 0;

  /// Total number of RECV work requests.
  uint64_t total_recv_requests_ = 0;
};
