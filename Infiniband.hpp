/**
 * \file Infiniband.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef INFINIBAND_HPP
#define INFINIBAND_HPP

#include <vector>
#include <cstring> // Required for memset().
#include <netdb.h>
#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>
#include <valgrind/memcheck.h>
#include "global.hpp"


/// InfiniBand exception class.
/** An InfinbandException object signals an error that occured in the
    InfiniBand communication functions. */

class InfinibandException : public std::runtime_error {
public:
    /// The InfinibandException default constructor.
    explicit InfinibandException(const std::string& what_arg = "")
        : std::runtime_error(what_arg) { }
};


/// InfiniBand connection base class.
/** An IBConnection object represents the endpoint of a single
    InfiniBand connection handled by an rdma connection manager. */

class IBConnection
{
public:

    /// The IBConnection constructor. Creates a connection manager ID.
    IBConnection(struct rdma_event_channel* ec, int index, struct rdma_cm_id* id = 0) :
        _index(index),
        _cm_id(id)
    {
        if (!_cm_id) {
            int err = rdma_create_id(ec, &_cm_id, this, RDMA_PS_TCP);
            if (err)
                throw InfinibandException("rdma_create_id failed");
        } else {
            _cm_id->context = this;
        }
            
        _qp_cap.max_send_wr = 16;
        _qp_cap.max_recv_wr = 16;
        _qp_cap.max_send_sge = 8;
        _qp_cap.max_recv_sge = 8;
        _qp_cap.max_inline_data = 0;
    };

    /// The IBConnection destructor.
    virtual ~IBConnection() {
        int err = rdma_destroy_id(_cm_id);
        if (err)
            throw InfinibandException("rdma_destroy_id() failed");
    }
    
    /// Retrieve the InfiniBand queue pair associated with the connection.
    struct ibv_qp* qp() const {
        return _cm_id->qp;
    };

    /// Initiate a connection request to target hostname and service.
    /**
       \param hostname The target hostname
       \param service  The target service or port number
    */
    void connect(const std::string& hostname,
                 const std::string& service) {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res;

        int err = getaddrinfo(hostname.c_str(), service.c_str(), &hints, &res);
        if (err)
            throw InfinibandException("getaddrinfo failed");

        out.debug() << "[" << _index << "] "
                    << "resolution of server address and route";

        for (struct addrinfo* t = res; t; t = t->ai_next) {
            err = rdma_resolve_addr(_cm_id, NULL, t->ai_addr,
                                    RESOLVE_TIMEOUT_MS);
            if (!err)
                break;
        }
        if (err)
            throw InfinibandException("rdma_resolve_addr failed");

        freeaddrinfo(res);
    }

    void disconnect() {
        out.debug() << "[" << _index << "] "
                    << "disconnect";
        int err = rdma_disconnect(_cm_id);
        if (err)
            throw InfinibandException("rdma_disconnect() failed");
    }
    
    /// Connection handler function, called on successful connection.
    /**
       \param event RDMA connection manager event structure
       \return      Non-zero if an error occured
    */
    virtual void on_established(struct rdma_cm_event* event) {
        out.debug() << "[" << _index << "] " << "connection established";
    }
    
    /// Handle RDMA_CM_EVENT_DISCONNECTED event for this connection.
    virtual void on_disconnected() {
        out.info() << "[" << _index << "] " << "connection disconnected";

        rdma_destroy_qp(_cm_id);
    }
    
    /// Handle RDMA_CM_EVENT_ADDR_RESOLVED event for this connection.
    virtual void on_addr_resolved(struct ibv_pd* pd, struct ibv_cq* cq) {
        out.debug() << "address resolved";

        struct ibv_qp_init_attr qp_attr;
        memset(&qp_attr, 0, sizeof qp_attr);
        qp_attr.cap = _qp_cap;
        qp_attr.send_cq = cq;
        qp_attr.recv_cq = cq;
        qp_attr.qp_type = IBV_QPT_RC;
        int err = rdma_create_qp(_cm_id, pd, &qp_attr);
        if (err)
            throw InfinibandException("creation of QP failed");
        
        err = rdma_resolve_route(_cm_id, RESOLVE_TIMEOUT_MS);
        if (err)
            throw InfinibandException("rdma_resolve_route failed");
    };

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event for this connection.
    virtual void on_connect_request(struct ibv_pd* pd, struct ibv_cq* cq) {

        struct ibv_qp_init_attr qp_attr;
        memset(&qp_attr, 0, sizeof qp_attr);
        qp_attr.cap = _qp_cap;
        qp_attr.send_cq = cq;
        qp_attr.recv_cq = cq;
        qp_attr.qp_type = IBV_QPT_RC;
        int err = rdma_create_qp(_cm_id, pd, &qp_attr);
        if (err)
            throw InfinibandException("creation of QP failed");
    };
    
    /// Handle RDMA_CM_EVENT_ROUTE_RESOLVED event for this connection.
    virtual void on_route_resolved() {
        out.debug() << "route resolved";

        struct rdma_conn_param conn_param;
        memset(&conn_param, 0, sizeof conn_param);
        conn_param.initiator_depth = 1;
        conn_param.retry_count = 7;
        int err = rdma_connect(_cm_id, &conn_param);
        if (err)
            throw InfinibandException("rdma_connect failed");
    };

    /// Retrieve index of this connection in the connection group.
    int index() const {
        return _index;
    };

    bool done() const {
        return _done;
    };

    /// Retrieve the total number of bytes transmitted.
    uint64_t total_bytes_sent() const {
        return _total_bytes_sent;
    }
    
    /// Retrieve the total number of SEND work requests.
    uint64_t total_send_requests() const {
        return _total_send_requests;
    }
    
    /// Retrieve the total number of RECV work requests.
    uint64_t total_recv_requests() const {
        return _total_recv_requests;
    }
    
protected:

    /// Index of this connection in a group of connections.
    int _index = 0;

    /// Flag indicating connection finished state.
    bool _done = false;

    /// Access information for a remote memory region.
    struct ServerInfo {
        uint64_t addr; ///< Target memory address
        uint32_t rkey; ///< Target remote access key
    };

    /// The queue pair capabilities.
    struct ibv_qp_cap _qp_cap;
    
    /// Post an InfiniBand SEND work request (WR) to the send queue
    void post_send(struct ibv_send_wr* wr) {
        struct ibv_send_wr* bad_send_wr;
        
        if (ibv_post_send(qp(), wr, &bad_send_wr))
            throw InfinibandException("ibv_post_send failed");

        _total_send_requests++;
        
        while (wr) {
            for (int i = 0; i < wr->num_sge; i++)
                _total_bytes_sent += wr->sg_list[i].length;
            wr = wr->next;
        }
    }

    /// Post an InfiniBand RECV work request (WR) to the receive queue.
    void post_recv(struct ibv_recv_wr* wr) {
        struct ibv_recv_wr* bad_recv_wr;
        
        if (ibv_post_recv(qp(), wr, &bad_recv_wr))
            throw InfinibandException("ibv_post_recv failed");

        _total_recv_requests++;
    }

    //private: TODO

    /// RDMA connection manager ID.
    struct rdma_cm_id* _cm_id = nullptr;
private:
    /// Total number of bytes transmitted.
    uint64_t _total_bytes_sent = 0;

    /// Total number of SEND work requests.
    uint64_t _total_send_requests = 0;

    /// Total number of RECV work requests.
    uint64_t _total_recv_requests = 0;

    /// Low-level communication parameters.
    enum {
        RESOLVE_TIMEOUT_MS = 5000 ///< Resolve timeout in milliseconds.
    };

};


/// InfiniBand connection group base class.
/** An IBConnectionGroup object represents a group of InfiniBand
    connections that use the same completion queue. */

template <typename CONNECTION>
class IBConnectionGroup
{
public:

    /// The IBConnectionGroup default constructor.
    IBConnectionGroup() {
        _ec = rdma_create_event_channel();
        if (!_ec)
            throw InfinibandException("rdma_create_event_channel failed");
    };

    /// The IBConnectionGroup default destructor.
    virtual ~IBConnectionGroup() {
        if (_listen_id) {
            int err = rdma_destroy_id(_listen_id);
            if (err)
                throw InfinibandException("rdma_destroy_id() failed");
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
    
    /// Initiate connection requests to list of target hostnames.
    /**
       \param hostnames The list of target hostnames
       \param services  The list of target services or port numbers
    */
    void connect(const std::vector<std::string>& hostnames,
                 const std::vector<std::string>& services) {
        for (unsigned int i = 0; i < hostnames.size(); i++) {
            CONNECTION* connection = new CONNECTION(_ec, i);
            _conn.push_back(connection);
            connection->connect(hostnames[i], services[i]);
        }
    };

    void accept(unsigned short port) {
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
        err = rdma_listen(_listen_id, 1);
        if (err)
            throw InfinibandException("RDMA listen failed");

        out.info() << "Waiting for connection";
    }

    /// Initiate disconnection.
    void disconnect() {
        for (auto c : _conn)
            c->disconnect();
    };

    /// The connection manager event loop.
    void handle_cm_events(bool is_connect = true) {
        int err;
        struct rdma_cm_event* event;
        struct rdma_cm_event event_copy;
        void* private_data_copy = 0;
        while ((err = rdma_get_cm_event(_ec, &event)) == 0) {
            VALGRIND_MAKE_MEM_DEFINED(event, sizeof(struct rdma_cm_event));
            memcpy(&event_copy, event, sizeof(struct rdma_cm_event));
            if (event_copy.param.conn.private_data) {
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
            if (_connected == (is_connect ? _conn.size() : 0))
                break;
        };
        if (err)
            throw InfinibandException("rdma_get_cm_event failed");
        
        out.info() << "number of connections: " << _connected;
    };

    /// The InfiniBand completion notification event loop.
    void completion_handler() {
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

        out.info() << "COMPLETION loop done";
    }

    /// Retrieve the InfiniBand protection domain.
    struct ibv_pd* protection_domain() const {
        return _pd;
    }

    /// Retrieve the InfiniBand completion queue.
    struct ibv_cq* completion_queue() const {
        return _cq;
    }

protected:

    /// InfiniBand protection domain.
    struct ibv_pd* _pd = nullptr;

    /// Vector of associated connection objects.
    std::vector<CONNECTION*> _conn;

    /// Flag causing termination of completion handler.
    bool _all_done = false;

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

    /// Handle RDMA_CM_EVENT_ESTABLISHED event.
    virtual void on_established(struct rdma_cm_event* event) {
        CONNECTION* conn = (CONNECTION*) event->id->context;

        conn->on_established(event);
        _connected++;
    }

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event.
    virtual void on_connect_request(struct rdma_cm_id* id) {
        if (!_pd)
            init_context(id->verbs);

        CONNECTION* conn = new CONNECTION(_ec, 0, id);
        _conn.push_back(conn);
        conn->on_connect_request(_pd, _cq);
    }
    
    /// Handle RDMA_CM_EVENT_DISCONNECTED event.
    virtual void on_disconnected(struct rdma_cm_id* id) {
        CONNECTION* conn = (CONNECTION*) id->context;

        conn->on_disconnected();
        _conn[conn->index()] = 0;
        delete conn;
        _connected--;
    }

private:

    /// Number of established connections
    unsigned int _connected = 0;

    /// RDMA event channel
    struct rdma_event_channel* _ec = nullptr;

    /// InfiniBand verbs context
    struct ibv_context* _context = nullptr;

    /// InfiniBand completion channel
    struct ibv_comp_channel* _comp_channel = nullptr;

    /// InfiniBand completion queue
    struct ibv_cq* _cq = nullptr;

    struct rdma_cm_id* _listen_id = nullptr;

    /// Connection manager event dispatcher. Called by the CM event loop.
    void on_cm_event(struct rdma_cm_event* event) {
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
            throw InfinibandException("request rejected by remote endpoint");
        case RDMA_CM_EVENT_ESTABLISHED:
            on_established(event);
            return;
        case RDMA_CM_EVENT_CONNECT_REQUEST:
            on_connect_request(event->id);
            return;
        case RDMA_CM_EVENT_DISCONNECTED:
            on_disconnected(event->id);
            return;
        default:
            out.error() << rdma_event_str(event->event);
        }
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

        _cq = ibv_create_cq(context, 40, NULL, _comp_channel, 0);
        if (!_cq)
            throw InfinibandException("ibv_create_cq failed");

        if (ibv_req_notify_cq(_cq, 0))
            throw InfinibandException("ibv_req_notify_cq failed");
    }

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) { };
};


#endif /* INFINIBAND_HPP */
