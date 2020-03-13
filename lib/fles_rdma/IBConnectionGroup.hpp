// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ConnectionGroupWorker.hpp"
#include "InfinibandException.hpp"
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <rdma/rdma_cma.h>
#include <sstream>
#include <valgrind/memcheck.h>
#include <vector>

/// InfiniBand connection group base class.
/** An IBConnectionGroup object represents a group of InfiniBand
    connections that use the same completion queue. */

template <typename CONNECTION>
class IBConnectionGroup : public ConnectionGroupWorker {
public:
  /// The IBConnectionGroup default constructor.
  IBConnectionGroup() {
    ec_ = rdma_create_event_channel();
    if (ec_ == nullptr) {
      throw InfinibandException("rdma_create_event_channel failed");
    }
    fcntl(ec_->fd, F_SETFL, O_NONBLOCK);
  }

  IBConnectionGroup(const IBConnectionGroup&) = delete;
  IBConnectionGroup& operator=(const IBConnectionGroup&) = delete;

  /// The IBConnectionGroup default destructor.
  ~IBConnectionGroup() override {
    for (auto& c : conn_) {
      c = nullptr;
    }

    if (listen_id_) {
      int err = rdma_destroy_id(listen_id_);
      if (err != 0) {
        L_(error) << "rdma_destroy_id() failed";
      }
      listen_id_ = nullptr;
    }

    if (cq_) {
      int err = ibv_destroy_cq(cq_);
      if (err != 0) {
        L_(error) << "ibv_destroy_cq() failed";
      }
      cq_ = nullptr;
    }

    if (pd_) {
      int err = ibv_dealloc_pd(pd_);
      if (err != 0) {
        L_(error) << "ibv_dealloc_pd() failed";
      }
      pd_ = nullptr;
    }

    rdma_destroy_event_channel(ec_);
  }

  void accept(unsigned short port, unsigned int count) {
    conn_.resize(count);

    L_(debug) << "Setting up RDMA CM structures";

    // Create rdma id (for listening)
    int err = rdma_create_id(ec_, &listen_id_, nullptr, RDMA_PS_TCP);
    if (err != 0) {
      L_(error) << "rdma_create_id() failed";
      throw InfinibandException("id creation failed");
    }

    // Bind rdma id (for listening) to socket address (local port)
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    sin.sin_port = htons(port); // NOLINT
    sin.sin_addr.s_addr = INADDR_ANY;
#pragma GCC diagnostic pop
    err = rdma_bind_addr(listen_id_, reinterpret_cast<struct sockaddr*>(&sin));
    if (err != 0) {
      L_(error) << "rdma_bind_addr(port=" << port
                << ") failed: " << strerror(errno);
      throw InfinibandException("RDMA bind_addr failed");
    }

    // Listen for connection request on rdma id
    err = rdma_listen(listen_id_, count);
    if (err != 0) {
      L_(error) << "rdma_listen() failed";
      throw InfinibandException("RDMA listen failed");
    }

    L_(debug) << "waiting for " << count << " connections";
  }

  /// Initiate disconnection.
  void disconnect() {
    for (auto& c : conn_) {
      c->disconnect();
    }
  }

  /// The connection manager event handler.
  void poll_cm_events() {
    int err;
    struct rdma_cm_event* event;
    struct rdma_cm_event event_copy;
    void* private_data_copy = nullptr;

    while ((err = rdma_get_cm_event(ec_, &event)) == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
      VALGRIND_MAKE_MEM_DEFINED(event, sizeof(struct rdma_cm_event));
      memcpy(&event_copy, event, sizeof(struct rdma_cm_event));
      if (event_copy.param.conn.private_data != nullptr) {
        VALGRIND_MAKE_MEM_DEFINED(event_copy.param.conn.private_data,
                                  event_copy.param.conn.private_data_len);
        private_data_copy = malloc(event_copy.param.conn.private_data_len);
        if (private_data_copy == nullptr) {
          throw InfinibandException("malloc failed");
        }
        memcpy(private_data_copy, event_copy.param.conn.private_data,
               event_copy.param.conn.private_data_len);
        event_copy.param.conn.private_data = private_data_copy;
      }
#pragma GCC diagnostic pop
      rdma_ack_cm_event(event);
      on_cm_event(&event_copy);
      if (private_data_copy != nullptr) {
        free(private_data_copy);
        private_data_copy = nullptr;
      }
    }
    if (err == -1 && errno == EAGAIN) {
      return;
    }
    if (err != 0) {
      throw InfinibandException("rdma_get_cm_event failed");
    }
  }

  /// The InfiniBand completion notification handler.
  int poll_completion() {
    const int ne_max = 10;

    struct ibv_wc wc[ne_max];
    int ne;
    int ne_total = 0;

    while (ne_total < 1000 && (ne = ibv_poll_cq(cq_, ne_max, wc))) {
      if (ne < 0) {
        throw InfinibandException("ibv_poll_cq failed");
      }

      ne_total += ne;
      for (int i = 0; i < ne; ++i) {
        if (wc[i].status != IBV_WC_SUCCESS) {
          std::ostringstream s;
          s << ibv_wc_status_str(wc[i].status) << " for wr_id "
            << static_cast<int>(wc[i].wr_id);
          L_(error) << s.str();

          continue;
        }

        on_completion(wc[i]);
      }
    }

    return ne_total;
  }

  /// Retrieve the InfiniBand protection domain.
  struct ibv_pd* protection_domain() const {
    return pd_;
  }

  /// Retrieve the InfiniBand completion queue.
  struct ibv_cq* completion_queue() const {
    return cq_;
  }

  size_t size() const { return conn_.size(); }

  /// Retrieve the total number of bytes transmitted.
  uint64_t aggregate_bytes_sent() const { return aggregate_bytes_sent_; }

  /// Retrieve the total number of SEND work requests.
  uint64_t aggregate_send_requests() const { return aggregate_send_requests_; }

  /// Retrieve the total number of RECV work requests.
  uint64_t aggregate_recv_requests() const { return aggregate_recv_requests_; }

  void summary() const {
    double runtime = std::chrono::duration_cast<std::chrono::microseconds>(
                         time_end_ - time_begin_)
                         .count();
    L_(info) << "summary: " << aggregate_send_requests_ << " SEND, "
             << aggregate_recv_requests_ << " RECV requests";
    double rate = static_cast<double>(aggregate_bytes_sent_) / runtime;
    L_(info) << "summary: " << human_readable_count(aggregate_bytes_sent_)
             << " sent in " << runtime / 1000000. << " s (" << rate << " MB/s)";
  }

protected:
  /// Handle RDMA_CM_EVENT_ADDR_RESOLVED event.
  virtual void on_addr_resolved(struct rdma_cm_id* id) {
    if (pd_ == nullptr) {
      init_context(id->verbs);
    }

    CONNECTION* conn = static_cast<CONNECTION*>(id->context);

    conn->on_addr_resolved(pd_, cq_);
  }

  /// Handle RDMA_CM_EVENT_ROUTE_RESOLVED event.
  virtual void on_route_resolved(struct rdma_cm_id* id) {
    CONNECTION* conn = static_cast<CONNECTION*>(id->context);

    conn->on_route_resolved();
  }

  /// Handle RDMA_CM_REJECTED event.
  virtual void on_rejected(struct rdma_cm_event* /* event */) {}

  /// Handle RDMA_CM_EVENT_ESTABLISHED event.
  virtual void on_established(struct rdma_cm_event* event) {
    CONNECTION* conn = static_cast<CONNECTION*>(event->id->context);

    conn->on_established(event);
    ++connected_;
  }

  /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
  virtual void on_connect_request(struct rdma_cm_event* /* event */) {}

  /// Handle RDMA_CM_EVENT_DISCONNECTED event.
  virtual void on_disconnected(struct rdma_cm_event* event) {
    CONNECTION* conn = static_cast<CONNECTION*>(event->id->context);

    aggregate_bytes_sent_ += conn->total_bytes_sent();
    aggregate_send_requests_ += conn->total_send_requests();
    aggregate_recv_requests_ += conn->total_recv_requests();

    conn->on_disconnected(event);
    --connected_;
    ++timewait_;
  }

  /// Handle RDMA_CM_EVENT_TIMEWAIT_EXIT event.
  virtual void on_timewait_exit(struct rdma_cm_event* event) {
    CONNECTION* conn = static_cast<CONNECTION*>(event->id->context);

    conn->on_timewait_exit(event);
    --timewait_;
  }

  /// Initialize the InfiniBand verbs context.
  void init_context(struct ibv_context* context) {
    context_ = context;

    L_(debug) << "create verbs objects";

    pd_ = ibv_alloc_pd(context);
    if (pd_ == nullptr) {
      throw InfinibandException("ibv_alloc_pd failed");
    }

    cq_ = ibv_create_cq(context, num_cqe_, nullptr, nullptr, 0);
    if (cq_ == nullptr) {
      throw InfinibandException("ibv_create_cq failed");
    }

    if (ibv_req_notify_cq(cq_, 0)) {
      throw InfinibandException("ibv_req_notify_cq failed");
    }
  }

  const uint32_t num_cqe_ = 1000000;

  /// InfiniBand protection domain.
  struct ibv_pd* pd_ = nullptr;

  /// InfiniBand completion queue
  struct ibv_cq* cq_ = nullptr;

  /// Vector of associated connection objects.
  std::vector<std::unique_ptr<CONNECTION>> conn_;

  /// Number of established connections
  unsigned int connected_ = 0;

  /// Number of connections in the timewait state.
  unsigned int timewait_ = 0;

  /// Number of connections in the done state.
  unsigned int connections_done_ = 0;

  /// Flag causing termination of completion handler.
  bool all_done_ = false;

  /// RDMA event channel
  struct rdma_event_channel* ec_ = nullptr;

  std::chrono::high_resolution_clock::time_point time_begin_;

  std::chrono::high_resolution_clock::time_point time_end_;

  Scheduler scheduler_;

private:
  /// Connection manager event dispatcher. Called by the CM event loop.
  void on_cm_event(struct rdma_cm_event* event) {
    L_(trace) << rdma_event_str(event->event);
    switch (event->event) {
    case RDMA_CM_EVENT_ADDR_RESOLVED:
      on_addr_resolved(event->id);
      return;
    case RDMA_CM_EVENT_ADDR_ERROR:
      L_(fatal) << rdma_event_str(event->event);
      throw InfinibandException("rdma_resolve_addr failed");
    case RDMA_CM_EVENT_ROUTE_RESOLVED:
      on_route_resolved(event->id);
      return;
    case RDMA_CM_EVENT_ROUTE_ERROR:
      L_(fatal) << rdma_event_str(event->event);
      throw InfinibandException("rdma_resolve_route failed");
    case RDMA_CM_EVENT_CONNECT_ERROR:
      L_(fatal) << rdma_event_str(event->event);
      throw InfinibandException("could not establish connection");
    case RDMA_CM_EVENT_UNREACHABLE:
      L_(fatal) << rdma_event_str(event->event);
      throw InfinibandException("remote server is not reachable");
    case RDMA_CM_EVENT_REJECTED:
      on_rejected(event);
      return;
    case RDMA_CM_EVENT_ESTABLISHED:
      on_established(event);
      return;
    case RDMA_CM_EVENT_CONNECT_REQUEST:
      on_connect_request(event);
      return;
    case RDMA_CM_EVENT_DISCONNECTED:
      on_disconnected(event);
      return;
    case RDMA_CM_EVENT_TIMEWAIT_EXIT:
      on_timewait_exit(event);
      return;
    default:
      L_(warning) << rdma_event_str(event->event);
    }
  }

  /// Completion notification event dispatcher. Called by the event loop.
  virtual void on_completion(const struct ibv_wc& wc) = 0;

  /// InfiniBand verbs context
  struct ibv_context* context_ = nullptr;

  struct rdma_cm_id* listen_id_ = nullptr;

  /// Total number of bytes transmitted.
  uint64_t aggregate_bytes_sent_ = 0;

  /// Total number of SEND work requests.
  uint64_t aggregate_send_requests_ = 0;

  /// Total number of RECV work requests.
  uint64_t aggregate_recv_requests_ = 0;
};
