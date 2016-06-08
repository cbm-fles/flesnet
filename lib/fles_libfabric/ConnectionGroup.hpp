// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "LibfabricException.hpp"
#include "Scheduler.hpp"
#include "ThreadContainer.hpp"
#include "Utility.hpp"
#include "Provider.hpp"
//#include <chrono>
//#include <cstring>
//#include <fcntl.h>
#include <log.hpp>
//#include <rdma/rdma_cma.h>
#include <rdma/fi_errno.h>
//#include <sstream>
#include <valgrind/memcheck.h>
//#include <vector>

#include <rdma/fi_domain.h>

#include <chrono>

/// Libfabric connection group base class.
/** An ConnectionGroup object represents a group of Libfabric
    connections that use the same completion queue. */

template <typename CONNECTION> class ConnectionGroup : public ThreadContainer
{
public:
    /// The ConnectionGroup default constructor.
    ConnectionGroup()
    {
      //std::cout << "ConnectionGroup constructor" << std::endl;
      struct fi_eq_attr eq_attr;
      memset(&eq_attr, sizeof(eq_attr), 0);
      eq_attr.size = 10;
      eq_attr.wait_obj = FI_WAIT_NONE;
      int res = fi_eq_open(Provider::getInst()->get_fabric(), &eq_attr, &eq_, nullptr);
      if (res)
        throw LibfabricException("fi_eq_open failed");

    }

    ConnectionGroup(const ConnectionGroup &) = delete;
    ConnectionGroup &operator=(const ConnectionGroup &) = delete;

    /// The ConnectionGroup default destructor.
    virtual ~ConnectionGroup()
    {
      for (auto& c : conn_)
        c = nullptr;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
      fi_close((fid_t)eq_);
      if(pep_ != nullptr)
        fi_close((fid_t)pep_);
#pragma GCC diagnostic pop

      pep_ = nullptr;
    }

  void accept(const std::string& hostname, unsigned short port, unsigned int count)
    {
        conn_.resize(count);
        Provider::getInst()->accept(pep_, hostname, port, count, eq_);

        L_(debug) << "waiting for " << count << " connections";
    }

    /// Initiate disconnection.
    void disconnect()
    {
        for (auto &c : conn_)
            c->disconnect();
    }

    /// The connection manager event handler.
    void poll_cm_events()
    {
        const size_t max_private_data_size =
            256; // verbs: 56, usnic and socket 256
        const size_t event_size =
            sizeof(struct fi_eq_cm_entry) + max_private_data_size;
        char buffer[event_size];

        uint32_t event_kind;

        ssize_t count = fi_eq_read(eq_, &event_kind, buffer, event_size, 0);
        if(count > 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
          on_cm_event(event_kind, (struct fi_eq_cm_entry*)buffer, count);
#pragma GCC diagnostic pop
        } else if (count == -FI_EAGAIN) {
          return;
        } else if(count == -FI_EAVAIL) {
          struct fi_eq_err_entry err_event;

          memset(&err_event, 0, sizeof(err_event));

          count = fi_eq_readerr(eq_, &err_event, 0);
          if(count > 0) {
            switch(err_event.err) {
            case ECONNREFUSED: {
              on_rejected(&err_event);
              break;
            }
            default:
              throw LibfabricException("unknown error in fi_eq_readerr");
            }
          }
        } else {
            throw LibfabricException("fi_eq_read failed");
        }
    }

    /// The Libfabric completion notification handler.
    int poll_completion()
    {
        const int ne_max = 10;

        struct fi_cq_entry wc[ne_max];
        int ne;
        int ne_total = 0;

        while ((ne = fi_cq_read(cq_, &wc, ne_max))) {
          if ((ne < 0) && (ne != -FI_EAGAIN)) {
                throw LibfabricException("fi_cq_read failed");
          }

          if(ne == -FI_EAGAIN)
            break;

            ne_total += ne;
            for (int i = 0; i < ne; ++i) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
              //L_(trace) << "on_completion(wr_id=" << (uintptr_t)wc[i].op_context << ")";
                on_completion((uintptr_t)wc[i].op_context);
#pragma GCC diagnostic pop
            }
        }

        return ne_total;
    }

    /// Retrieve the InfiniBand completion queue.
    struct fid_cq *completion_queue() const
    {
        return cq_;
    }

    size_t size() const
    {
        return conn_.size();
    }

    /// Retrieve the total number of bytes transmitted.
    uint64_t aggregate_bytes_sent() const
    {
        return aggregate_bytes_sent_;
    }

    /// Retrieve the total number of SEND work requests.
    uint64_t aggregate_send_requests() const
    {
        return aggregate_send_requests_;
    }

    /// Retrieve the total number of RECV work requests.
    uint64_t aggregate_recv_requests() const
    {
        return aggregate_recv_requests_;
    }

    void summary() const
    {
        double runtime = std::chrono::duration_cast<std::chrono::microseconds>(
            time_end_ - time_begin_).count();
        L_(info) << "summary: " << aggregate_send_requests_ << " SEND, "
                 << aggregate_recv_requests_ << " RECV requests";
        double rate = static_cast<double>(aggregate_bytes_sent_) / runtime;
        L_(info) << "summary: " << human_readable_count(aggregate_bytes_sent_)
                 << " sent in " << runtime / 1000000. << " s (" << rate
                 << " MB/s)";
    }

    /// The "main" function of an ConnectionGroup decendant.
    virtual void operator()() = 0;

protected:

    /// Handle RDMA_CM_REJECTED event.
    virtual void on_rejected(struct fi_eq_err_entry* /* event */) {}

    virtual void on_connected(struct fid_domain* /*pd*/) { };

    /// Handle RDMA_CM_EVENT_ESTABLISHED event.
    virtual void on_established(struct fi_eq_cm_entry *event)
    {
        CONNECTION *conn = static_cast<CONNECTION *>(event->fid->context);

        conn->on_established(event);
        ++connected_;
        on_connected(pd_);
    }

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
    virtual void on_connect_request(struct fi_eq_cm_entry * /* event */,
                                    size_t /* private_data_len */) = 0;

    /// Handle RDMA_CM_EVENT_DISCONNECTED event.
    virtual void on_disconnected(struct fi_eq_cm_entry *event)
    {
        CONNECTION *conn = static_cast<CONNECTION *>(event->fid->context);

        aggregate_bytes_sent_ += conn->total_bytes_sent();
        aggregate_send_requests_ += conn->total_send_requests();
        aggregate_recv_requests_ += conn->total_recv_requests();

        conn->on_disconnected(event);
        --connected_;
    }

    /// Initialize the Libfabric context.
    void init_context(fi_info *info)
    {
      static int vector = 0;

        L_(debug) << "create Libfabric objects";

        int res = fi_domain(Provider::getInst()->get_fabric(),
                            info, &pd_, nullptr);
        if (!pd_)
            throw LibfabricException("fi_domain failed");

        struct fi_cq_attr cq_attr;
        memset(&cq_attr, sizeof(cq_attr), 0);
        cq_attr.size = num_cqe_;
        cq_attr.flags = 0;
        cq_attr.format = FI_CQ_FORMAT_CONTEXT;
        cq_attr.wait_obj = FI_WAIT_NONE;
        cq_attr.signaling_vector = vector++; // ??
        cq_attr.wait_cond = FI_CQ_COND_NONE;
        cq_attr.wait_set = nullptr;
        res = fi_cq_open(pd_, &cq_attr, &cq_, nullptr);
        if (!cq_) {
          std::cout << strerror(-res) << std::endl;
            throw LibfabricException("fi_cq_open failed");
        }
    }

    const uint32_t num_cqe_ = 1000000;

    /// Libfabric protection domain.
    struct fid_domain* pd_ = nullptr;

    /// Libfabric completion queue
    struct fid_cq *cq_ = nullptr;

    /// Vector of associated connection objects.
    std::vector<std::unique_ptr<CONNECTION> > conn_;

    /// Number of established connections
    unsigned int connected_ = 0;

    /// Number of connections in the done state.
    unsigned int connections_done_ = 0;

    /// Flag causing termination of completion handler.
    bool all_done_ = false;

    /// RDMA event channel
    struct fid_eq *eq_ = nullptr;

    std::chrono::high_resolution_clock::time_point time_begin_;

    std::chrono::high_resolution_clock::time_point time_end_;

    Scheduler scheduler_;

private:
    /// Connection manager event dispatcher. Called by the CM event loop.
    void on_cm_event(uint32_t event_kind, struct fi_eq_cm_entry *event,
                     ssize_t event_size)
    {
      //L_(trace) << rdma_event_str(event_kind);
        switch (event_kind) {
        case FI_CONNECTED:
            on_established(event);
            break;
        case FI_CONNREQ:
            on_connect_request(event,
                               event_size - sizeof(struct fi_eq_cm_entry));
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

    /// Total number of SEND work requests.
    uint64_t aggregate_send_requests_ = 0;

    /// Total number of RECV work requests.
    uint64_t aggregate_recv_requests_ = 0;

    /// RDMA connection manager ID.
    struct fid_pep *pep_ = nullptr;
};
