/**
 * \file IBConnectionGroup.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef IBCONNECTIONGROUP_HPP
#define IBCONNECTIONGROUP_HPP

/// InfiniBand connection group base class.
/** An IBConnectionGroup object represents a group of InfiniBand
    connections that use the same completion queue. */

template <typename CONNECTION>
class IBConnectionGroup : public ThreadContainer
{
public:
    IBConnectionGroup(const IBConnectionGroup&) = delete;
    IBConnectionGroup& operator=(const IBConnectionGroup&) = delete;

    /// The IBConnectionGroup default constructor.
    IBConnectionGroup() {
        _ec = rdma_create_event_channel();
        if (!_ec)
            throw InfinibandException("rdma_create_event_channel failed");
    };

    /// The IBConnectionGroup default destructor.
    virtual ~IBConnectionGroup() {
        for (auto& c : _conn)
            c = nullptr;

        if (_listen_id) {
            int err = rdma_destroy_id(_listen_id);
            if (err)
                throw InfinibandException("rdma_destroy_id failed");
            _listen_id = nullptr;
        }

        if (_cq) {
            int err = ibv_destroy_cq(_cq);
            if (err)
                throw InfinibandException("ibv_destroy_cq failed");
            _cq = nullptr;
        }

        if (_comp_channel) {
            int err = ibv_destroy_comp_channel(_comp_channel);
            if (err)
                throw InfinibandException("ibv_destroy_comp_channel failed");
            _comp_channel = nullptr;
        }

        if (_pd) {
            int err = ibv_dealloc_pd(_pd);
            if (err)
                throw InfinibandException("ibv_dealloc_pd failed");
            _pd = nullptr;
        }

        rdma_destroy_event_channel(_ec);
    };
    
    void accept(unsigned short port, unsigned int count) {
        _conn.resize(count);

        out.debug() << "Setting up RDMA CM structures";

        // Create rdma id (for listening)
        int err = rdma_create_id(_ec, &_listen_id, NULL, RDMA_PS_TCP);
        if (err)
            throw InfinibandException("id creation failed");

        // Bind rdma id (for listening) to socket address (local port)
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof sin);
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = INADDR_ANY;
        err = rdma_bind_addr(_listen_id, (struct sockaddr*) & sin);
        if (err)
            throw InfinibandException("RDMA bind_addr failed");

        // Listen for connection request on rdma id
        err = rdma_listen(_listen_id, count);
        if (err)
            throw InfinibandException("RDMA listen failed");

        out.debug() << "waiting for " << count << " connections";
    }

    /// Initiate disconnection.
    void disconnect() {
        for (auto& c : _conn)
            c->disconnect();
    };

    /// The connection manager event loop.
    void handle_cm_events(unsigned int target_num_connections) {
        set_cpu(0);

        int err;
        struct rdma_cm_event* event;
        struct rdma_cm_event event_copy;
        void* private_data_copy = 0;
        while ((err = rdma_get_cm_event(_ec, &event)) == 0) {
            VALGRIND_MAKE_MEM_DEFINED(event, sizeof(struct rdma_cm_event));
            memcpy(&event_copy, event, sizeof(struct rdma_cm_event));
            if (event_copy.param.conn.private_data) {
                VALGRIND_MAKE_MEM_DEFINED(event_copy.param.conn.private_data,
                                          event_copy.param.conn.private_data_len);
                private_data_copy = malloc(event_copy.param.conn.private_data_len);
                if (!private_data_copy)
                    throw InfinibandException("malloc failed");
                memcpy(private_data_copy, event_copy.param.conn.private_data,
                       event_copy.param.conn.private_data_len);
                event_copy.param.conn.private_data = private_data_copy;
            }
            rdma_ack_cm_event(event);
            on_cm_event(&event_copy);
            if (private_data_copy) {
                free(private_data_copy);
                private_data_copy = 0;
            }
            if (_connected == target_num_connections)
                break;
        };
        if (err)
            throw InfinibandException("rdma_get_cm_event failed");
        
        out.debug() << "number of connections: " << _connected;
    };

    /// The InfiniBand completion notification event loop.
    void completion_handler() {
        set_cpu(1);

        const int ne_max = 10;

        struct ibv_cq* ev_cq;
        void* ev_ctx;
        struct ibv_wc wc[ne_max];
        int ne;

        while (!_all_done) {
            if (ibv_get_cq_event(_comp_channel, &ev_cq, &ev_ctx))
                throw InfinibandException("ibv_get_cq_event failed");

            ibv_ack_cq_events(ev_cq, 1);

            if (ev_cq != _cq)
                throw InfinibandException("CQ event for unknown CQ");

            if (ibv_req_notify_cq(_cq, 0))
                throw InfinibandException("ibv_req_notify_cq failed");

            while ((ne = ibv_poll_cq(_cq, ne_max, wc))) {
                if (ne < 0)
                    throw InfinibandException("ibv_poll_cq failed");

                for (int i = 0; i < ne; i++) {
                    if (wc[i].status != IBV_WC_SUCCESS) {
                        std::ostringstream s;
                        s << ibv_wc_status_str(wc[i].status)
                          << " for wr_id " << (int) wc[i].wr_id;
                        out.error() << s.str();

                        continue;
                    }

                    on_completion(wc[i]);
                }
            }
        }

        out.debug() << "COMPLETION loop done";
    }

    /// Retrieve the InfiniBand protection domain.
    struct ibv_pd* protection_domain() const {
        return _pd;
    }

    /// Retrieve the InfiniBand completion queue.
    struct ibv_cq* completion_queue() const {
        return _cq;
    }

    size_t size() const {
        return _conn.size();
    }

    /// Retrieve the total number of bytes transmitted.
    uint64_t aggregate_bytes_sent() const {
        return _aggregate_bytes_sent;
    }

    /// Retrieve the total number of SEND work requests.
    uint64_t aggregate_send_requests() const {
        return _aggregate_send_requests;
    }

    /// Retrieve the total number of RECV work requests.
    uint64_t aggregate_recv_requests() const {
        return _aggregate_recv_requests;
    }

    void summary(double runtime) const {
        out.info() << "summary: " << _aggregate_send_requests << " SEND, "
                   << _aggregate_recv_requests << " RECV requests";
        double rate = (double) _aggregate_bytes_sent / runtime;
        out.info() << "summary: " << _aggregate_bytes_sent
                   << " bytes sent in "
                   << runtime/1000000. << " s (" << rate << " MB/s)";
    }

protected:

    /// InfiniBand protection domain.
    struct ibv_pd* _pd = nullptr;

    /// InfiniBand completion queue
    struct ibv_cq* _cq = nullptr;

    /// Vector of associated connection objects.
    std::vector<std::unique_ptr<CONNECTION> > _conn;

    /// Number of established connections
    unsigned int _connected = 0;

    /// Number of connections in the done state.
    unsigned int _connections_done = 0;

    /// Flag causing termination of completion handler.
    bool _all_done = false;

    /// RDMA event channel
    struct rdma_event_channel* _ec = nullptr;

    /// Handle RDMA_CM_EVENT_ADDR_RESOLVED event.
    virtual void on_addr_resolved(struct rdma_cm_id* id) {
        if (!_pd)
            init_context(id->verbs);

        CONNECTION* conn = (CONNECTION*) id->context;
        
        conn->on_addr_resolved(_pd, _cq);
    }

    /// Handle RDMA_CM_EVENT_ROUTE_RESOLVED event.
    virtual void on_route_resolved(struct rdma_cm_id* id) {
        CONNECTION* conn = (CONNECTION*) id->context;

        conn->on_route_resolved();
    }

    /// Handle RDMA_CM_REJECTED event.
    virtual void on_rejected(struct rdma_cm_event* event) { };

    /// Handle RDMA_CM_EVENT_ESTABLISHED event.
    virtual void on_established(struct rdma_cm_event* event) {
        CONNECTION* conn = (CONNECTION*) event->id->context;

        conn->on_established(event);
        _connected++;
    }

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
    virtual void on_connect_request(struct rdma_cm_event* event) { };
    
    /// Handle RDMA_CM_EVENT_DISCONNECTED event.
    virtual void on_disconnected(struct rdma_cm_event* event) {
        CONNECTION* conn = (CONNECTION*) event->id->context;

        _aggregate_bytes_sent += conn->total_bytes_sent();
        _aggregate_send_requests += conn->total_send_requests();
        _aggregate_recv_requests += conn->total_recv_requests();

        conn->on_disconnected(event);
        _connected--;
    }

    /// Initialize the InfiniBand verbs context.
    void init_context(struct ibv_context* context) {
        _context = context;

        out.debug() << "create verbs objects";

        _pd = ibv_alloc_pd(context);
        if (!_pd)
            throw InfinibandException("ibv_alloc_pd failed");

        _comp_channel = ibv_create_comp_channel(context);
        if (!_comp_channel)
            throw InfinibandException("ibv_create_comp_channel failed");

        _cq = ibv_create_cq(context, par->num_cqe(), NULL, _comp_channel, 0);
        if (!_cq)
            throw InfinibandException("ibv_create_cq failed");

        if (ibv_req_notify_cq(_cq, 0))
            throw InfinibandException("ibv_req_notify_cq failed");
    }

private:

    /// InfiniBand verbs context
    struct ibv_context* _context = nullptr;

    /// InfiniBand completion channel
    struct ibv_comp_channel* _comp_channel = nullptr;

    struct rdma_cm_id* _listen_id = nullptr;

    /// Total number of bytes transmitted.
    uint64_t _aggregate_bytes_sent = 0;

    /// Total number of SEND work requests.
    uint64_t _aggregate_send_requests = 0;

    /// Total number of RECV work requests.
    uint64_t _aggregate_recv_requests = 0;

    /// Connection manager event dispatcher. Called by the CM event loop.
    void on_cm_event(struct rdma_cm_event* event) {
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
};


#endif /* IBCONNECTIONGROUP_HPP */
