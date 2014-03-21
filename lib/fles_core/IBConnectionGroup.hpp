// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ThreadContainer.hpp"
#include "InfinibandException.hpp"
#include "Utility.hpp"
#include "global.hpp"
#include <chrono>
#include <vector>
#include <thread>
#include <cstring>
#include <rdma/rdma_cma.h>
#include <valgrind/memcheck.h>

/// InfiniBand connection group base class.
/** An IBConnectionGroup object represents a group of InfiniBand
    connections that use the same completion queue. */

template <typename CONNECTION> class IBConnectionGroup : public ThreadContainer
{
public:
    /// The IBConnectionGroup default constructor.
    IBConnectionGroup()
    {
        _ec = rdma_create_event_channel();
        if (!_ec)
            throw InfinibandException("rdma_create_event_channel failed");
    }

    IBConnectionGroup(const IBConnectionGroup&) = delete;
    IBConnectionGroup& operator=(const IBConnectionGroup&) = delete;

    /// The IBConnectionGroup default destructor.
    virtual ~IBConnectionGroup()
    {
        for (auto& c : _conn)
            c = nullptr;

        if (_listen_id) {
            int err = rdma_destroy_id(_listen_id);
            if (err) {
                out.error() << "rdma_destroy_id() failed";
            }
            _listen_id = nullptr;
        }

        if (_cq) {
            int err = ibv_destroy_cq(_cq);
            if (err) {
                out.error() << "ibv_destroy_cq() failed";
            }
            _cq = nullptr;
        }

        if (_pd) {
            int err = ibv_dealloc_pd(_pd);
            if (err) {
                out.error() << "ibv_dealloc_pd() failed";
            }
            _pd = nullptr;
        }

        rdma_destroy_event_channel(_ec);
    }

    void accept(unsigned short port, unsigned int count)
    {
        _conn.resize(count);

        out.debug() << "Setting up RDMA CM structures";

        // Create rdma id (for listening)
        int err = rdma_create_id(_ec, &_listen_id, nullptr, RDMA_PS_TCP);
        if (err) {
            out.error() << "rdma_create_id() failed";
            throw InfinibandException("id creation failed");
        }

        // Bind rdma id (for listening) to socket address (local port)
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof sin);
        sin.sin_family = AF_INET;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = INADDR_ANY;
#pragma GCC diagnostic pop
        err = rdma_bind_addr(_listen_id,
                             reinterpret_cast<struct sockaddr*>(&sin));
        if (err) {
            out.error() << "rdma_bind_addr(port=" << port
                        << ") failed: " << strerror(errno);
            throw InfinibandException("RDMA bind_addr failed");
        }

        // Listen for connection request on rdma id
        err = rdma_listen(_listen_id, count);
        if (err) {
            out.error() << "rdma_listen() failed";
            throw InfinibandException("RDMA listen failed");
        }

        out.debug() << "waiting for " << count << " connections";
    }

    /// Initiate disconnection.
    void disconnect()
    {
        for (auto& c : _conn)
            c->disconnect();
    }

    /// The connection manager event loop.
    /// The thread main function.
    void handle_cm_events(unsigned int target_num_connections)
    {
        try
        {
            set_cpu(0);

            int err;
            struct rdma_cm_event* event;
            struct rdma_cm_event event_copy;
            void* private_data_copy = nullptr;
            while ((err = rdma_get_cm_event(_ec, &event)) == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
                VALGRIND_MAKE_MEM_DEFINED(event, sizeof(struct rdma_cm_event));
                memcpy(&event_copy, event, sizeof(struct rdma_cm_event));
                if (event_copy.param.conn.private_data) {
                    VALGRIND_MAKE_MEM_DEFINED(
                        event_copy.param.conn.private_data,
                        event_copy.param.conn.private_data_len);
                    private_data_copy =
                        malloc(event_copy.param.conn.private_data_len);
                    if (!private_data_copy)
                        throw InfinibandException("malloc failed");
                    memcpy(private_data_copy,
                           event_copy.param.conn.private_data,
                           event_copy.param.conn.private_data_len);
                    event_copy.param.conn.private_data = private_data_copy;
                }
#pragma GCC diagnostic pop
                rdma_ack_cm_event(event);
                on_cm_event(&event_copy);
                if (private_data_copy) {
                    free(private_data_copy);
                    private_data_copy = nullptr;
                }
                if (_connected == target_num_connections)
                    break;
            }
            if (err)
                throw InfinibandException("rdma_get_cm_event failed");

            out.debug() << "number of connections: " << _connected;
        }
        catch (std::exception& e)
        {
            out.error() << "exception in handle_cm_events(): " << e.what();
        }
    }

    /// The InfiniBand completion notification event loop.
    /// The thread main function.
    void completion_handler()
    {
        try
        {
            set_cpu(1);

            _time_begin = std::chrono::high_resolution_clock::now();

            const int ne_max = 10;

            struct ibv_wc wc[ne_max];
            int ne;

            while (!_all_done) {
                while ((ne = ibv_poll_cq(_cq, ne_max, wc))) {
                    if (ne < 0)
                        throw InfinibandException("ibv_poll_cq failed");

                    for (int i = 0; i < ne; ++i) {
                        if (wc[i].status != IBV_WC_SUCCESS) {
                            std::ostringstream s;
                            s << ibv_wc_status_str(wc[i].status)
                              << " for wr_id " << static_cast<int>(wc[i].wr_id);
                            out.error() << s.str();

                            continue;
                        }

                        on_completion(wc[i]);
                    }
                }
            }

            _time_end = std::chrono::high_resolution_clock::now();

            out.debug() << "COMPLETION loop done";
        }
        catch (std::exception& e)
        {
            out.error() << "exception in completion_handler(): " << e.what();
        }
    }

    /// Retrieve the InfiniBand protection domain.
    struct ibv_pd* protection_domain() const { return _pd; }

    /// Retrieve the InfiniBand completion queue.
    struct ibv_cq* completion_queue() const { return _cq; }

    size_t size() const { return _conn.size(); }

    /// Retrieve the total number of bytes transmitted.
    uint64_t aggregate_bytes_sent() const { return _aggregate_bytes_sent; }

    /// Retrieve the total number of SEND work requests.
    uint64_t aggregate_send_requests() const
    {
        return _aggregate_send_requests;
    }

    /// Retrieve the total number of RECV work requests.
    uint64_t aggregate_recv_requests() const
    {
        return _aggregate_recv_requests;
    }

    void summary() const
    {
        double runtime = std::chrono::duration_cast<std::chrono::microseconds>(
                             _time_end - _time_begin).count();
        out.info() << "summary: " << _aggregate_send_requests << " SEND, "
                   << _aggregate_recv_requests << " RECV requests";
        double rate = static_cast<double>(_aggregate_bytes_sent) / runtime;
        out.info() << "summary: "
                   << human_readable_byte_count(_aggregate_bytes_sent)
                   << " sent in " << runtime / 1000000. << " s (" << rate
                   << " MB/s)";
    }

    /// The "main" function of an IBConnectionGroup decendant.
    virtual void operator()() = 0;

protected:
    /// Handle RDMA_CM_EVENT_ADDR_RESOLVED event.
    virtual void on_addr_resolved(struct rdma_cm_id* id)
    {
        if (!_pd)
            init_context(id->verbs);

        CONNECTION* conn = static_cast<CONNECTION*>(id->context);

        conn->on_addr_resolved(_pd, _cq);
    }

    /// Handle RDMA_CM_EVENT_ROUTE_RESOLVED event.
    virtual void on_route_resolved(struct rdma_cm_id* id)
    {
        CONNECTION* conn = static_cast<CONNECTION*>(id->context);

        conn->on_route_resolved();
    }

    /// Handle RDMA_CM_REJECTED event.
    virtual void on_rejected(struct rdma_cm_event* /* event */) {}

    /// Handle RDMA_CM_EVENT_ESTABLISHED event.
    virtual void on_established(struct rdma_cm_event* event)
    {
        CONNECTION* conn = static_cast<CONNECTION*>(event->id->context);

        conn->on_established(event);
        ++_connected;
    }

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
    virtual void on_connect_request(struct rdma_cm_event* /* event */) {}

    /// Handle RDMA_CM_EVENT_DISCONNECTED event.
    virtual void on_disconnected(struct rdma_cm_event* event)
    {
        CONNECTION* conn = static_cast<CONNECTION*>(event->id->context);

        _aggregate_bytes_sent += conn->total_bytes_sent();
        _aggregate_send_requests += conn->total_send_requests();
        _aggregate_recv_requests += conn->total_recv_requests();

        conn->on_disconnected(event);
        _connected--;
    }

    /// Initialize the InfiniBand verbs context.
    void init_context(struct ibv_context* context)
    {
        _context = context;

        out.debug() << "create verbs objects";

        _pd = ibv_alloc_pd(context);
        if (!_pd)
            throw InfinibandException("ibv_alloc_pd failed");

        _cq = ibv_create_cq(context, _num_cqe, nullptr, nullptr, 0);
        if (!_cq)
            throw InfinibandException("ibv_create_cq failed");

        if (ibv_req_notify_cq(_cq, 0))
            throw InfinibandException("ibv_req_notify_cq failed");
    }

    const uint32_t _num_cqe = 1000000;

    /// InfiniBand protection domain.
    struct ibv_pd* _pd = nullptr;

    /// InfiniBand completion queue
    struct ibv_cq* _cq = nullptr;

    /// Vector of associated connection objects.
    std::vector<std::unique_ptr<CONNECTION>> _conn;

    /// Number of established connections
    unsigned int _connected = 0;

    /// Number of connections in the done state.
    unsigned int _connections_done = 0;

    /// Flag causing termination of completion handler.
    bool _all_done = false;

    /// RDMA event channel
    struct rdma_event_channel* _ec = nullptr;

    std::chrono::high_resolution_clock::time_point _time_begin;

    std::chrono::high_resolution_clock::time_point _time_end;

private:
    /// Connection manager event dispatcher. Called by the CM event loop.
    void on_cm_event(struct rdma_cm_event* event)
    {
        out.trace() << rdma_event_str(event->event);
        switch (event->event) {
        case RDMA_CM_EVENT_ADDR_RESOLVED:
            on_addr_resolved(event->id);
            return;
        case RDMA_CM_EVENT_ADDR_ERROR:
            throw InfinibandException("rdma_resolve_addr failed");
        case RDMA_CM_EVENT_ROUTE_RESOLVED:
            on_route_resolved(event->id);
            return;
        case RDMA_CM_EVENT_ROUTE_ERROR:
            throw InfinibandException("rdma_resolve_route failed");
        case RDMA_CM_EVENT_CONNECT_ERROR:
            throw InfinibandException("could not establish connection");
        case RDMA_CM_EVENT_UNREACHABLE:
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
        default:
            out.error() << rdma_event_str(event->event);
        }
    }

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) = 0;

    /// InfiniBand verbs context
    struct ibv_context* _context = nullptr;

    struct rdma_cm_id* _listen_id = nullptr;

    /// Total number of bytes transmitted.
    uint64_t _aggregate_bytes_sent = 0;

    /// Total number of SEND work requests.
    uint64_t _aggregate_send_requests = 0;

    /// Total number of RECV work requests.
    uint64_t _aggregate_recv_requests = 0;
};
