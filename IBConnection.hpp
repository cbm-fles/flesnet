/**
 * \file IBConnection.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef IBCONNECTION_HPP
#define IBCONNECTION_HPP

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
    IBConnection(const IBConnection&) = delete;
    IBConnection& operator=(const IBConnection&) = delete;

    /// The IBConnection constructor. Creates a connection manager ID.
    IBConnection(struct rdma_event_channel* ec, uint_fast16_t index,
                 struct rdma_cm_id* id = nullptr) :
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
        if (_cm_id) {
            int err = rdma_destroy_id(_cm_id);
            if (err)
                throw InfinibandException("rdma_destroy_id failed");
            _cm_id = nullptr;
        }
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
        out.debug() << "[" << _index << "] " << "disconnect";
        int err = rdma_disconnect(_cm_id);
        if (err)
            throw InfinibandException("rdma_disconnect failed");
    }
    
    virtual void on_rejected(struct rdma_cm_event* event) {
        out.debug() << "[" << _index << "] " << "connection rejected";

        rdma_destroy_qp(_cm_id);
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
    virtual void on_disconnected(struct rdma_cm_event* event) {
        out.debug() << "[" << _index << "] " << "connection disconnected";

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

        setup(pd);
    };

    virtual void create_qp(struct ibv_pd* pd, struct ibv_cq* cq) {
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

    virtual void accept_connect_request() {
        out.debug() << "accepting connection";

        // Accept rdma connection request
        auto private_data = get_private_data();
        assert(private_data->size() <= 255);

        struct rdma_conn_param conn_param = {};
        conn_param.responder_resources = 1;
        conn_param.private_data = private_data->data();
        conn_param.private_data_len = (uint8_t) private_data->size();
        int err = rdma_accept(_cm_id, &conn_param);
        if (err)
            throw InfinibandException("RDMA accept failed");
    };

    /// Handle RDMA_CM_EVENT_CONNECT_REQUEST event for this connection.
    virtual void on_connect_request(struct rdma_cm_event* event,
                                    struct ibv_pd* pd, struct ibv_cq* cq) {
        create_qp(pd, cq);
        setup(pd);
        accept_connect_request();
    };

    virtual std::unique_ptr<std::vector<uint8_t>> get_private_data() {
        std::unique_ptr<std::vector<uint8_t> >
            private_data(new std::vector<uint8_t>());

        return private_data;
    };

    virtual void setup(struct ibv_pd* pd) = 0;

    /// Handle RDMA_CM_EVENT_ROUTE_RESOLVED event for this connection.
    virtual void on_route_resolved() {
        out.debug() << "route resolved";

        // Initiate rdma connection
        auto private_data = get_private_data();
        assert(private_data->size() <= 255);

        struct rdma_conn_param conn_param;
        memset(&conn_param, 0, sizeof conn_param);
        conn_param.initiator_depth = 1;
        conn_param.retry_count = 7;
        conn_param.private_data = private_data->data();
        conn_param.private_data_len = (uint8_t) private_data->size();
        int err = rdma_connect(_cm_id, &conn_param);
        if (err) {
            out.fatal() << "rdma_connect failed: " << strerror(err);
            throw InfinibandException("rdma_connect failed");
        }
    };

    /// Retrieve index of this connection in the connection group.
    uint_fast16_t index() const {
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
    uint_fast16_t _index = UINT_FAST16_MAX;

    /// Flag indicating connection finished state.
    bool _done = false;

    /// The queue pair capabilities.
    struct ibv_qp_cap _qp_cap;

    void dump_send_wr(struct ibv_send_wr* wr) {
        for (int i = 0; wr; i++, wr = wr->next) {
            out.fatal() << "wr[" << i << "]: wr_id=" << wr->wr_id
                        << " (" << (REQUEST_ID) wr->wr_id << ")";
            out.fatal() << " opcode=" << wr->opcode;
            out.fatal() << " send_flags=" << (ibv_send_flags) wr->send_flags;
            out.fatal() << " num_sge=" << wr->num_sge;
            for (int j = 0; j < wr->num_sge; j++) {
                out.fatal() << "  sg_list[" << j << "] "
                            << "addr=" << wr->sg_list[j].addr;
                out.fatal() << "  sg_list[" << j << "] "
                            << "length=" << wr->sg_list[j].length;
                out.fatal() << "  sg_list[" << j << "] "
                            << "lkey=" << wr->sg_list[j].lkey;
            }
        }
    }

    /// Post an InfiniBand SEND work request (WR) to the send queue
    void post_send(struct ibv_send_wr* wr) {
        struct ibv_send_wr* bad_send_wr;

        int err = ibv_post_send(qp(), wr, &bad_send_wr);
        if (err) {
            out.fatal() << "ibv_post_send failed: " << strerror(err);
            dump_send_wr(wr);
            out.fatal() << "previous send requests: " << _total_send_requests;
            out.fatal() << "previous recv requests: " << _total_recv_requests;
            throw InfinibandException("ibv_post_send failed");
        }

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

        int err = ibv_post_recv(qp(), wr, &bad_recv_wr);
        if (err) {
            out.fatal() << "ibv_post_recv failed: " << strerror(err);
            throw InfinibandException("ibv_post_recv failed");
        }

        _total_recv_requests++;
    }

private:

    /// RDMA connection manager ID.
    struct rdma_cm_id* _cm_id = nullptr;

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


#endif /* IBCONNECTION_HPP */
