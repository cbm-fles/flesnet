/*
 * Infiniband.hpp
 *
 * 2012, Jan de Cuveland
 */

#ifndef INFINIBAND_HPP
#define INFINIBAND_HPP

#include <vector>

#include "log.hpp"


class IBConnection {
public:
    typedef struct {
        uint64_t addr;
        uint32_t rkey;
    } ServerInfo;

    ServerInfo _serverInfo[2];

    IBConnection(struct rdma_event_channel* ec, int index) : _index(index) {
        int err = rdma_create_id(ec, &_cmId, this, RDMA_PS_TCP);
        if (err)
            throw ApplicationException("rdma_create_id failed");
    };

    struct ibv_qp* qp() const {
        return _cmId->qp;
    };

    void
    initiateConnect(const char* hostname, const char* service) {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res;

        int err = getaddrinfo(hostname, service, &hints, &res);
        if (err)
            throw ApplicationException("getaddrinfo failed");

        Log.debug() << "[" << _index << "] "
                    << "resolution of server address and route";

        for (struct addrinfo* t = res; t; t = t->ai_next) {
            err = rdma_resolve_addr(_cmId, NULL, t->ai_addr,
                                    RESOLVE_TIMEOUT_MS);
            if (!err)
                break;
        }
        if (err)
            throw ApplicationException("rdma_resolve_addr failed");

        freeaddrinfo(res);
    }

    int onConnection(struct rdma_cm_event* event) {
        memcpy(&_serverInfo, event->param.conn.private_data,
               sizeof _serverInfo);

        Log.debug() << "[" << _index << "] "
                    << "connection established";

        return 0;
    }

private:
    struct rdma_cm_id* _cmId;
protected:
    int _index;
};


template <typename T>
class IBConnectionGroup {
public:
    std::vector<T*> _conn;
    unsigned int _connected;
    struct rdma_event_channel* _ec;
    struct ibv_comp_channel* _comp_channel;
    struct ibv_context* _context;
    struct ibv_pd* _pd;
    struct ibv_cq* _cq;

    IBConnectionGroup() :
        _connected(0), _ec(0), _comp_channel(0), _context(0), _pd(0),
        _cq(0) {
        _ec = rdma_create_event_channel();
        if (!_ec)
            throw ApplicationException("rdma_create_event_channel failed");
    };

    ~IBConnectionGroup() {
        rdma_destroy_event_channel(_ec);
    }

    void initiateConnect(std::vector<std::string> hostnames) {
        for (unsigned int i = 0; i < hostnames.size(); i++) {

            T* connection = new T(_ec, i);
            _conn.push_back(connection);

            const char* hostname = hostnames[i].c_str();

            char* service;
            if (asprintf(&service, "%d", BASE_PORT + i) < 0)
                throw ApplicationException("invalid port");

            connection->initiateConnect(hostname, service);

            free(service);
        }

        // cm event loop
        {
            int err;
            struct rdma_cm_event* event;

            while ((err = rdma_get_cm_event(_ec, &event)) == 0) {
                int err = onCmEvent(event);
                rdma_ack_cm_event(event);
                if (err || _conn.size() == _connected)
                    break;
            };
            if (err)
                throw ApplicationException("rdma_get_cm_event failed");

            Log.info() << "number of connections: " << _connected;
        }
    };


    int onCmEvent(struct rdma_cm_event* event) {
        switch (event->event) {

        case RDMA_CM_EVENT_ADDR_RESOLVED:
            return onAddrResolved(event->id);
        case RDMA_CM_EVENT_ADDR_ERROR:
            throw ApplicationException("rdma_resolve_addr failed");
        case RDMA_CM_EVENT_ROUTE_RESOLVED:
            return onRouteResolved(event->id);
        case RDMA_CM_EVENT_ROUTE_ERROR:
            throw ApplicationException("rdma_resolve_route failed");
        case RDMA_CM_EVENT_CONNECT_ERROR:
            throw ApplicationException("could not establish connection");
        case RDMA_CM_EVENT_UNREACHABLE:
            throw ApplicationException("remote server is not reachable");
        case RDMA_CM_EVENT_REJECTED:
            throw ApplicationException("request rejected by remote endpoint");
        case RDMA_CM_EVENT_ESTABLISHED:
            return onConnection(event);
        case RDMA_CM_EVENT_DISCONNECTED:
            throw ApplicationException("connection has been disconnected");
        case RDMA_CM_EVENT_CONNECT_REQUEST:
        case RDMA_CM_EVENT_CONNECT_RESPONSE:
        case RDMA_CM_EVENT_DEVICE_REMOVAL:
        case RDMA_CM_EVENT_MULTICAST_JOIN:
        case RDMA_CM_EVENT_MULTICAST_ERROR:
        case RDMA_CM_EVENT_ADDR_CHANGE:
        case RDMA_CM_EVENT_TIMEWAIT_EXIT:
        default:
            throw ApplicationException("unknown cm event");
        }
    }


    void init_ibv_context(struct ibv_context* context) {
        _context = context;

        Log.debug() << "create verbs objects";

        _pd = ibv_alloc_pd(context);
        if (!_pd)
            throw ApplicationException("ibv_alloc_pd failed");

        _comp_channel = ibv_create_comp_channel(context);
        if (!_comp_channel)
            throw ApplicationException("ibv_create_comp_channel failed");

        _cq = ibv_create_cq(context, 40, NULL, _comp_channel, 0);
        if (!_cq)
            throw ApplicationException("ibv_create_cq failed");

        if (ibv_req_notify_cq(_cq, 0))
            throw ApplicationException("ibv_req_notify_cq failed");
    }


    int onAddrResolved(struct rdma_cm_id* id) {
        Log.debug() << "address resolved";

        if (!_pd)
            init_ibv_context(id->verbs);

        struct ibv_qp_init_attr qp_attr;
        memset(&qp_attr, 0, sizeof qp_attr);
        qp_attr.cap.max_send_wr = 20;
        qp_attr.cap.max_send_sge = 8;
        qp_attr.cap.max_recv_wr = 20;
        qp_attr.cap.max_recv_sge = 8;
        qp_attr.cap.max_inline_data = sizeof(tscdesc_t) * 10;
        qp_attr.send_cq = _cq;
        qp_attr.recv_cq = _cq;
        qp_attr.qp_type = IBV_QPT_RC;
        int err = rdma_create_qp(id, _pd, &qp_attr);
        if (err)
            throw ApplicationException("creation of QP failed");

        T* conn = (T*) id->context;
        conn->onAddrResolved(_pd);

        err = rdma_resolve_route(id, RESOLVE_TIMEOUT_MS);
        if (err)
            throw ApplicationException("rdma_resolve_route failed");

        return 0;
    }


    int onRouteResolved(struct rdma_cm_id* id) {
        Log.debug() << "route resolved";

        struct rdma_conn_param conn_param;
        memset(&conn_param, 0, sizeof conn_param);
        conn_param.initiator_depth = 1;
        conn_param.retry_count = 7;
        int err = rdma_connect(id, &conn_param);
        if (err)
            throw ApplicationException("rdma_connect failed");

        return 0;
    }


    int onConnection(struct rdma_cm_event* event) {
        T* conn = (T*) event->id->context;

        conn->onConnection(event);
        _connected++;

        return 0;
    }

    virtual void onCompletion(struct ibv_wc& wc) { };

    void
    completionHandler() {
        const int ne_max = 10;

        struct ibv_cq* ev_cq;
        void* ev_ctx;
        struct ibv_wc wc[ne_max];
        int ne;

        while (true) {
            if (ibv_get_cq_event(_comp_channel, &ev_cq, &ev_ctx))
                throw ApplicationException("ibv_get_cq_event failed");

            ibv_ack_cq_events(ev_cq, 1);

            if (ev_cq != _cq)
                throw ApplicationException("CQ event for unknown CQ");

            if (ibv_req_notify_cq(_cq, 0))
                throw ApplicationException("ibv_req_notify_cq failed");

            while ((ne = ibv_poll_cq(_cq, ne_max, wc))) {
                if (ne < 0)
                    throw ApplicationException("ibv_poll_cq failed");

                for (int i = 0; i < ne; i++) {
                    if (wc[i].status != IBV_WC_SUCCESS) {
                        std::ostringstream s;
                        s << ibv_wc_status_str(wc[i].status)
                          << " for wr_id " << (int) wc[i].wr_id;
                        throw ApplicationException(s.str());
                    }

                    onCompletion(wc[i]);
                }
            }
        }
    }

};


#endif /* INFINIBAND_HPP */
