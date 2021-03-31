// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include "ConnectionGroupWorker.hpp"
#include "RequestIdentifier.hpp"
#include "dfs/SchedulerOrchestrator.hpp"
#include "log.hpp"
#include "providers/LibfabricBarrier.hpp"
#include "providers/LibfabricContextPool.hpp"
#include "providers/LibfabricException.hpp"
#include "providers/Provider.hpp"

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>
#include <set>

#include <chrono>
#include <fstream>

#include <sys/uio.h>

namespace tl_libfabric {
/// Libfabric connection group base class.
/** An ConnectionGroup object represents a group of Libfabric
 connections that use the same completion queue. */

template <typename CONNECTION>
class ConnectionGroup : public ConnectionGroupWorker {
public:
  /// The ConnectionGroup default constructor.
  ConnectionGroup(std::string local_node_name) {
    Provider::init(local_node_name);
    // std::cout << "ConnectionGroup constructor" << std::endl;
    struct fi_eq_attr eq_attr;
    memset(&eq_attr, 0, sizeof(eq_attr));
    eq_attr.size = 10;
    eq_attr.wait_obj = FI_WAIT_NONE;
    int res =
        fi_eq_open(Provider::getInst()->get_fabric(), &eq_attr, &eq_, nullptr);
    if (res) {
      L_(fatal) << "fi_eq_open failed: " << res << "=" << fi_strerror(-res);
      throw LibfabricException("fi_eq_open failed");
    }
    cqs_.resize(MAX_CQ_INSTANCE);
  }

  ConnectionGroup(const ConnectionGroup&) = delete;
  ConnectionGroup& operator=(const ConnectionGroup&) = delete;

  /// The ConnectionGroup default destructor.
  ~ConnectionGroup() override {
    for (auto& c : conn_) {
      c = nullptr;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    fi_close((fid_t)eq_);
    if (pep_ != nullptr)
      fi_close((fid_t)pep_);
#pragma GCC diagnostic pop

    pep_ = nullptr;
  }

  void
  accept(const std::string& hostname, unsigned short port, unsigned int count) {
    conn_.resize(count);
    Provider::getInst()->accept(pep_, hostname, port, count, eq_);

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
    const size_t max_private_data_size = 256; // verbs: 56, usnic and socket 256
    const size_t event_size =
        sizeof(struct fi_eq_cm_entry) + max_private_data_size;
    char buffer[event_size];

    uint32_t event_kind;

    ssize_t count = fi_eq_read(eq_, &event_kind, buffer, event_size, 0);
    if (count > 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
      on_cm_event(event_kind, (struct fi_eq_cm_entry*)buffer, count);
#pragma GCC diagnostic pop
    } else if (count == -FI_EAGAIN) {
      return;
    } else if (count == -FI_EAVAIL) {
      struct fi_eq_err_entry err_event;

      memset(&err_event, 0, sizeof(err_event));

      count = fi_eq_readerr(eq_, &err_event, 0);
      if (count > 0) {
        switch (err_event.err) {
        case ECONNREFUSED: {
          on_rejected(&err_event);
          break;
        }
        case ETIMEDOUT: {
          // on_rejected(&err_event);
          break;
        }
        default: {
          L_(fatal) << "unknown error in fi_eq_readerr: " << err_event.err
                    << fi_strerror(err_event.err);
          throw LibfabricException("unknown error in fi_eq_readerr");
        }
        }
      }
    } else {
      throw LibfabricException("fi_eq_read failed");
    }
  }

  /// The Libfabric completion notification handler.
  int poll_completion() {
    // TODO detect the number of messages that we are waiting for
    const int ne_max = conn_.size() * conn_.size() * 1000;

    struct fi_cq_tagged_entry wc[ne_max];
    int ne;
    int ne_total = 0;

    agg_CQ_count_++;
    std::chrono::high_resolution_clock::time_point start, end;

    for (uint16_t i = 0; i < MAX_CQ_INSTANCE; i++) {
      start = std::chrono::high_resolution_clock::now();
      ne = fi_cq_read(cqs_[i], wc, ne_max);
      if (ne != 0) {
        /// LOGGING
        end = std::chrono::high_resolution_clock::now();
        assert(end >= start);
        uint64_t diff =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();
        agg_CQ_time_ += diff;
        /// END OF LOGGING
        if (ne == -FI_EAVAIL) { // error available
          struct fi_cq_err_entry err;
          char buffer[256];
          ne = fi_cq_readerr(cqs_[i], &err, 0);
          // err == 0 --> Success
          if (err.err != 0 && err.err != 5) {
            L_(fatal) << fi_strerror(err.err);
            L_(fatal) << fi_cq_strerror(cqs_[i], err.prov_errno, err.err_data,
                                        buffer, 256)
                      << ("fi_cq_read[" + std::to_string(i) +
                          "] failed (fi_cq_readerr) " + buffer +
                          "[code:" + std::to_string(err.err) + "]" +
                          " ne: " + std::to_string(ne));
            throw LibfabricException("fi_cq_read[" + std::to_string(i) +
                                     "] failed (fi_cq_readerr) " + buffer +
                                     "[code:" + std::to_string(err.err) + "]" +
                                     " ne: " + std::to_string(ne));
          }
        }
        if ((ne < 0) && (ne != -FI_EAGAIN)) {
          L_(fatal) << "fi_cq_read[" << i << "] failed: " << ne << "="
                    << fi_strerror(-ne);
          throw LibfabricException("fi_cq_read failed");
        }

        if (ne != -FI_EAGAIN) {

          ne_total += ne;
          if (ne > 0) {
            start = std::chrono::high_resolution_clock::now();
          }
          for (int i = 0; i < ne; ++i) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
            struct fi_custom_context* context =
                static_cast<struct fi_custom_context*>(wc[i].op_context);
            assert(context != nullptr);
            on_completion((uintptr_t)context->op_context);
            if (((uintptr_t)context->op_context & 0xFF) == ID_WRITE_DESC ||
                ((uintptr_t)context->op_context & 0xFF) == ID_WRITE_DATA ||
                ((uintptr_t)context->op_context & 0xFF) == ID_WRITE_DATA_WRAP)
              LibfabricContextPool::getInst()->releaseContext(context);
#pragma GCC diagnostic pop
          }
          if (ne > 0) {
            end = std::chrono::high_resolution_clock::now();
            assert(end >= start);
            agg_CQ_COMP_time_ +=
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start)
                    .count();
          }
        }
      }
    }

    return ne_total;
  }

  /// Retrieve the InfiniBand completion queue.
  struct fid_cq* completion_queue(uint32_t conn_index) const {
    return cqs_[conn_index % MAX_CQ_INSTANCE];
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
    L_(info) << "summary: Agg. bytes of sync messages "
             << human_readable_count(aggregate_sync_bytes_sent_);
    L_(info) << "summary: Agg. CQ retrieving time " << agg_CQ_time_ / 1000000.
             << " s and processing time " << agg_CQ_COMP_time_ / 1000000.
             << " s in " << agg_CQ_count_ << " calls";
  }

  /// The "main" function of an ConnectionGroup decendant.
  virtual void operator()() override = 0;

protected:
  /// Handle RDMA_CM_REJECTED event.
  virtual void on_rejected(struct fi_eq_err_entry* /* event */) {}

  virtual void on_connected(struct fid_domain* /*pd*/){};

  /// Handle RDMA_CM_EVENT_ESTABLISHED event.
  virtual void on_established(struct fi_eq_cm_entry* event) {
    CONNECTION* conn = static_cast<CONNECTION*>(event->fid->context);

    conn->on_established(event);
    ++connected_;
    on_connected(pd_);
  }

  /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
  virtual void on_connect_request(struct fi_eq_cm_entry* /* event */,
                                  size_t /* private_data_len */){};

  /// Handle RDMA_CM_EVENT_DISCONNECTED event.
  virtual void on_disconnected(struct fi_eq_cm_entry* event,
                               int conn_indx = -1) {
    if (conn_indx == -1) {
      CONNECTION* conn = static_cast<CONNECTION*>(event->fid->context);
      aggregate_bytes_sent_ += conn->total_bytes_sent();
      aggregate_sync_bytes_sent_ += conn->total_sync_bytes_sent();
      aggregate_send_requests_ += conn->total_send_requests();
      aggregate_recv_requests_ += conn->total_recv_requests();
      conn->on_disconnected(event);
    } else {
      aggregate_bytes_sent_ += conn_[conn_indx]->total_bytes_sent();
      aggregate_sync_bytes_sent_ += conn_[conn_indx]->total_sync_bytes_sent();
      aggregate_send_requests_ += conn_[conn_indx]->total_send_requests();
      aggregate_recv_requests_ += conn_[conn_indx]->total_recv_requests();
      conn_[conn_indx]->on_disconnected(event);
    }
    --connected_;
  }

  /// Initialize the Libfabric context.
  void init_context(fi_info* info,
                    const std::vector<std::string>& compute_hostnames,
                    const std::vector<std::string>& compute_services) {
    L_(debug) << "create Libfabric objects";

    int res = fi_domain(Provider::getInst()->get_fabric(), info, &pd_, nullptr);
    if (!pd_) {
      L_(fatal) << "fi_domain failed: " << -res << "=" << fi_strerror(-res);
      throw LibfabricException("fi_domain failed");
    }

    L_(debug) << "creating CQs ...";
    for (uint16_t i = 0; i < MAX_CQ_INSTANCE; i++) {
      struct fi_cq_attr cq_attr;
      memset(&cq_attr, 0, sizeof(cq_attr));
      cq_attr.size = num_cqe_;
      cq_attr.flags = 0;
      // cq_attr.format = FI_CQ_FORMAT_CONTEXT;
      cq_attr.format = FI_CQ_FORMAT_TAGGED;
      cq_attr.wait_obj = FI_WAIT_NONE;
      cq_attr.signaling_vector = Provider::vector++; // ??
      cq_attr.wait_cond = FI_CQ_COND_NONE;
      cq_attr.wait_set = nullptr;
      res = fi_cq_open(pd_, &cq_attr, &cqs_[i], nullptr);
      if (!cqs_[i]) {
        L_(fatal) << "fi_cq_open[" << i << "] failed: " << -res << "="
                  << fi_strerror(-res);
        throw LibfabricException("fi_cq_open failed");
      }
    }

    if (Provider::getInst()->has_av()) {
      struct fi_av_attr av_attr;

      memset(&av_attr, 0, sizeof(av_attr));
      av_attr.type = FI_AV_TABLE;
      av_attr.count = 1000;
      assert(av_ == nullptr);
      res = fi_av_open(pd_, &av_attr, &av_, NULL);
      assert(res == 0);
      if (av_ == nullptr) {
        L_(fatal) << "fi_av_open failed: " << res << "=" << fi_strerror(-res);
        throw LibfabricException("fi_av_open failed");
      }

      Provider::getInst()->set_hostnames_and_services(
          av_, compute_hostnames, compute_services, fi_addrs);
    }
  }

  void sync_buffer_positions() {
    for (auto& c : conn_) {
      c->try_sync_buffer_positions();
    }

    auto now = std::chrono::system_clock::now();
    scheduler_.add(std::bind(&ConnectionGroup::sync_buffer_positions, this),
                   now + std::chrono::milliseconds(0));
  }

  virtual void sync_heartbeat() = 0;

  //
  void send_heartbeat_to_inactive_connections() {
    std::vector<uint32_t> inactive_conns =
        SchedulerOrchestrator::retrieve_new_inactive_connections();
    for (uint32_t inactive : inactive_conns) {
      if (!conn_[inactive]->done()) {
        conn_[inactive]->prepare_heartbeat();
      }
    }
  }

  const uint32_t num_cqe_ = 1000000;

  /// Libfabric protection domain.
  struct fid_domain* pd_ = nullptr;

  /// Max number of completion queues (distributing diff. connections on
  /// multiple completion queues minimize the time to retrieve events)
  // TODO make it configurable
  uint16_t MAX_CQ_INSTANCE = 10;

  /// Libfabric completion queues
  std::vector<struct fid_cq*> cqs_;

  /// Libfabric address vector.
  struct fid_av* av_ = nullptr;

  /// Vector of associated connection objects.
  std::vector<std::unique_ptr<CONNECTION>> conn_;

  /// Number of established connections
  unsigned int connected_ = 0;
  std::set<int> connected_indexes_;

  /// Number of connections in the done state.
  unsigned int connections_done_ = 0;

  /// Flag causing termination of completion handler.
  bool all_done_ = false;

  /// RDMA event channel
  struct fid_eq* eq_ = nullptr;

  std::chrono::high_resolution_clock::time_point time_begin_;

  std::chrono::high_resolution_clock::time_point time_end_;

  Scheduler scheduler_;

  /// RDMA endpoint (for connection-less fabrics).
  struct fid_ep* ep_ = nullptr;

  /// AV indices for compute buffer nodes
  std::vector<fi_addr_t> fi_addrs = {};

  bool connection_oriented_ = false;

private:
  /// Connection manager event dispatcher. Called by the CM event loop.
  void on_cm_event(uint32_t event_kind,
                   struct fi_eq_cm_entry* event,
                   ssize_t event_size) {
    // L_(trace) << rdma_event_str(event_kind);
    switch (event_kind) {
    case FI_CONNECTED:
      on_established(event);
      break;
    case FI_CONNREQ:
      on_connect_request(event, event_size - sizeof(struct fi_eq_cm_entry));
      break;
    case FI_SHUTDOWN:
      on_disconnected(event);
      break;
    default:
      L_(warning) << "unknown eq event";
    }
  }

  /// Completion notification event dispatcher. Called by the event loop.
  virtual void on_completion(uint64_t wc) = 0;

  /// Total number of bytes transmitted.
  uint64_t aggregate_bytes_sent_ = 0;

  /// Total number of bytes transmitted.
  uint64_t aggregate_sync_bytes_sent_ = 0;
  /// Total number of SEND work requests.
  uint64_t aggregate_send_requests_ = 0;

  /// Total number of RECV work requests.
  uint64_t aggregate_recv_requests_ = 0;

  /// RDMA connection manager ID (for connection-oriented fabrics)
  struct fid_pep* pep_ = nullptr;

  /// LOGGING completion queues statistics
  uint64_t agg_CQ_time_ = 0;

  uint64_t agg_CQ_count_ = 0;

  uint64_t agg_CQ_COMP_time_ = 0;
};
} // namespace tl_libfabric
