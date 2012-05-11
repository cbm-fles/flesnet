/*
 * InputApplication.hpp
 *
 * 2012, Jan de Cuveland
 */

#ifndef INPUTAPPLICATION_HPP
#define INPUTAPPLICATION_HPP

#include <boost/thread.hpp>
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


class ComputeNodeConnection : public IBConnection {
public:
    ComputeNodeConnection(struct rdma_event_channel* ec, int index) :
        IBConnection(ec, index), _our_turn(1) {
        memset(&_receive_cn_ack, 0, sizeof(cn_bufpos_t));
        memset(&_cn_ack, 0, sizeof(cn_bufpos_t));
        memset(&_cn_wp, 0, sizeof(cn_bufpos_t));
        memset(&_send_cn_wp, 0, sizeof(cn_bufpos_t));
    }

    void
    onAddrResolved(struct ibv_pd* pd) {
        // register memory regions
        _mr_recv = ibv_reg_mr(pd, &_receive_cn_ack,
                              sizeof(cn_bufpos_t),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_recv)
            throw ApplicationException("registration of memory region failed");

        _mr_send = ibv_reg_mr(pd, &_send_cn_wp,
                              sizeof(cn_bufpos_t), 0);
        if (!_mr_send)
            throw ApplicationException("registration of memory region failed");

        setup_recv();
        setup_send();
        post_recv_cn_ack();
    }

    void setup_recv() {
        recv_sge.addr = (uintptr_t) &_receive_cn_ack;
        recv_sge.length = sizeof(cn_bufpos_t);
        recv_sge.lkey = _mr_recv->lkey;
        memset(&recv_wr, 0, sizeof recv_wr);
        recv_wr.wr_id = ID_RECEIVE_CN_ACK | (_index << 8);
        recv_wr.sg_list = &recv_sge;
        recv_wr.num_sge = 1;
    }

    void setup_send() {
        send_sge.addr = (uintptr_t) &_send_cn_wp;
        send_sge.length = sizeof(cn_bufpos_t);
        send_sge.lkey = _mr_send->lkey;
        memset(&send_wr, 0, sizeof send_wr);
        send_wr.wr_id = ID_SEND_CN_WP;
        send_wr.opcode = IBV_WR_SEND;
        send_wr.sg_list = &send_sge;
        send_wr.num_sge = 1;
    }

    void onCompleteRecv() {
        Log.debug()
                << "[" << _index << "] "
                << "receive completion, new _cn_ack.data="
                << _receive_cn_ack.data;
        {
            boost::mutex::scoped_lock lock(_cn_ack_mutex);
            _cn_ack = _receive_cn_ack;
            _cn_ack_cond.notify_one();
        }
        post_recv_cn_ack();
        {
            boost::mutex::scoped_lock lock(_cn_wp_mutex);
            if (_cn_wp.data != _send_cn_wp.data
                    || _cn_wp.desc != _send_cn_wp.desc) {
                _send_cn_wp = _cn_wp;
                post_send_cn_wp();
            } else {
                _our_turn = 1;
            }
        }
    }

    void post_recv_cn_ack() {
        // Post a receive work request (WR) to the receive queue
        Log.trace() << "[" << _index << "] "
                    << "POST RECEIVE _receive_cn_ack";
        if (ibv_post_recv(qp(), &recv_wr, &bad_recv_wr))
            throw ApplicationException("ibv_post_recv failed");
    }

    void post_send_cn_wp() {
        // Post a send work request (WR) to the send queue
        Log.trace() << "[" << _index << "] "
                    << "POST SEND _send_cp_wp (data=" << _send_cn_wp.data
                    << " desc=" << _send_cn_wp.desc << ")";
        if (ibv_post_send(qp(), &send_wr, &bad_send_wr))
            throw ApplicationException("ibv_post_send failed");
    }

    void post_send_data(struct ibv_sge* sge, int num_sge, uint64_t timeslice,
                        uint64_t mc_length, uint64_t data_length) {
        int num_sge2 = 0;
        struct ibv_sge sge2[4];

        uint64_t target_words_left =
            CN_DATABUF_WORDS - _cn_wp.data % CN_DATABUF_WORDS;

        // split sge list if necessary
        int num_sge_cut = 0;
        if (data_length + mc_length > target_words_left) {
            for (int i = 0; i < num_sge; i++) {
                if (sge[i].length <= (target_words_left * sizeof(uint64_t))) {
                    target_words_left -= sge[i].length / sizeof(uint64_t);
                } else {
                    if (target_words_left) {
                        sge2[num_sge2].addr =
                            sge[i].addr
                            + sizeof(uint64_t) * target_words_left;
                        sge2[num_sge2].length =
                            sge[i].length
                            - sizeof(uint64_t) * target_words_left;
                        sge2[num_sge2++].lkey = sge[i].lkey;
                        sge[i].length = sizeof(uint64_t) * target_words_left;
                        target_words_left = 0;
                    } else {
                        sge2[num_sge2++] = sge[i];
                        num_sge_cut++;
                    }
                }
            }
        }
        num_sge -= num_sge_cut;

        struct ibv_send_wr send_wr_ts, send_wr_tswrap, send_wr_tscdesc;
        memset(&send_wr_ts, 0, sizeof(send_wr_ts));
        send_wr_ts.wr_id = ID_WRITE_DATA;
        send_wr_ts.opcode = IBV_WR_RDMA_WRITE;
        send_wr_ts.sg_list = sge;
        send_wr_ts.num_sge = num_sge;
        send_wr_ts.wr.rdma.rkey = _serverInfo[0].rkey;
        send_wr_ts.wr.rdma.remote_addr =
            (uintptr_t)(_serverInfo[0].addr +
                        (_cn_wp.data % CN_DATABUF_WORDS) * sizeof(uint64_t));

        if (num_sge2) {
            memset(&send_wr_tswrap, 0, sizeof(send_wr_ts));
            send_wr_tswrap.wr_id = ID_WRITE_DATA_WRAP;
            send_wr_tswrap.opcode = IBV_WR_RDMA_WRITE;
            send_wr_tswrap.sg_list = sge2;
            send_wr_tswrap.num_sge = num_sge2;
            send_wr_tswrap.wr.rdma.rkey = _serverInfo[0].rkey;
            send_wr_tswrap.wr.rdma.remote_addr =
                (uintptr_t) _serverInfo[0].addr;
            send_wr_ts.next = &send_wr_tswrap;
            send_wr_tswrap.next = &send_wr_tscdesc;
        } else {
            send_wr_ts.next = &send_wr_tscdesc;
        }

        // timeslice component descriptor
        tscdesc_t tscdesc;
        tscdesc.ts_num = timeslice;
        tscdesc.offset = _cn_wp.data;
        tscdesc.size = data_length + mc_length;
        struct ibv_sge sge3;
        sge3.addr = (uintptr_t) &tscdesc;
        sge3.length = sizeof(tscdesc);
        sge3.lkey = 0;

        memset(&send_wr_tscdesc, 0, sizeof(send_wr_tscdesc));
        send_wr_tscdesc.wr_id = ID_WRITE_DESC | (timeslice << 8);
        send_wr_tscdesc.opcode = IBV_WR_RDMA_WRITE;
        send_wr_tscdesc.send_flags =
            IBV_SEND_INLINE | IBV_SEND_FENCE | IBV_SEND_SIGNALED;
        send_wr_tscdesc.sg_list = &sge3;
        send_wr_tscdesc.num_sge = 1;
        send_wr_tscdesc.wr.rdma.rkey = _serverInfo[1].rkey;
        send_wr_tscdesc.wr.rdma.remote_addr =
            (uintptr_t)(_serverInfo[1].addr
                        + (_cn_wp.desc % CN_DESCBUF_WORDS)
                        * sizeof(tscdesc_t));

        Log.debug() << "[" << _index << "] "
                    << "post_send (timeslice " << timeslice << ")";

        // send everything
        struct ibv_send_wr* bad_send_wr;
        if (ibv_post_send(qp(), &send_wr_ts, &bad_send_wr))
            throw ApplicationException("ibv_post_send failed");
    }

    // wait until enough space is available at target compute node
    void waitForBufferSpace(uint64_t dataSize, uint64_t descSize) {
        boost::mutex::scoped_lock lock(_cn_ack_mutex);
        Log.trace() << "[" << _index << "] "
                    << "SENDER data space (words) required="
                    << dataSize << ", avail="
                    << _cn_ack.data + CN_DATABUF_WORDS - _cn_wp.data;
        Log.trace() << "[" << _index << "] "
                    << "SENDER desc space (words) required="
                    << descSize << ", avail="
                    << _cn_ack.desc + CN_DESCBUF_WORDS - _cn_wp.desc;
        while (_cn_ack.data - _cn_wp.data + CN_DATABUF_WORDS < dataSize
                || _cn_ack.desc - _cn_wp.desc + CN_DESCBUF_WORDS
                < descSize) {
            {
                boost::mutex::scoped_lock lock2(_cn_wp_mutex);
                if (_our_turn) {
                    // send phony update to receive new pointers
                    {
                        Log.info() << "[" << _index << "] "
                                   << "SENDER send phony update";
                        _our_turn = 0;
                        _send_cn_wp = _cn_wp;
                        post_send_cn_wp();
                    }
                }
            }
            _cn_ack_cond.wait(lock);
            Log.trace() << "[" << _index << "] "
                        << "SENDER (next try) space avail="
                        << _cn_ack.data - _cn_wp.data + CN_DATABUF_WORDS
                        << " desc_avail=" << _cn_ack.desc - _cn_wp.desc
                        + CN_DESCBUF_WORDS;
        }
    }


    void incWritePointers(uint64_t dataSize, uint64_t descSize) {
        boost::mutex::scoped_lock lock(_cn_wp_mutex);
        _cn_wp.data += dataSize;
        _cn_wp.desc += descSize;
        if (_our_turn) {
            _our_turn = 0;
            _send_cn_wp = _cn_wp;
            post_send_cn_wp();
        }
    }

    struct ibv_mr* _mr_recv;
    struct ibv_mr* _mr_send;

    int _our_turn;

    cn_bufpos_t _receive_cn_ack;
    cn_bufpos_t _cn_ack;

    cn_bufpos_t _cn_wp;
    cn_bufpos_t _send_cn_wp;

    boost::mutex _cn_ack_mutex;
    boost::mutex _cn_wp_mutex;
    boost::condition_variable_any _cn_ack_cond;

    struct ibv_sge recv_sge;
    struct ibv_recv_wr recv_wr;
    struct ibv_recv_wr* bad_recv_wr;

    struct ibv_sge send_sge;
    struct ibv_send_wr send_wr;
    struct ibv_send_wr* bad_send_wr;
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


class InputBuffer : public IBConnectionGroup<ComputeNodeConnection> {
public:
    InputBuffer() :
        _acked_mc(0), _acked_data(0),
        _mc_written(0), _data_written(0) {
        _data = new uint64_t[DATA_WORDS]();
        _addr = new uint64_t[ADDR_WORDS]();
        _ack = new uint64_t[ACK_WORDS]();
    };

    ~InputBuffer() {
        delete [] _data;
        delete [] _addr;
        delete [] _ack;
    };

    void sender_thread() {
        sender_loop();
    };

    void
    setup() {
        // register memory regions
        _mr_data = ibv_reg_mr(_pd, _data,
                              DATA_WORDS * sizeof(uint64_t),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_data)
            throw ApplicationException("registration of memory region failed");

        _mr_addr = ibv_reg_mr(_pd, _addr,
                              ADDR_WORDS * sizeof(uint64_t),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_addr)
            throw ApplicationException("registration of memory region failed");
    }

private:
    int
    target_cn_index(uint64_t timeslice) {
        return timeslice % _conn.size();
    }

    void
    wait_for_data(uint64_t min_mc_number) {
        uint64_t mcs_to_write = min_mc_number - _mc_written;
        // write more data than requested (up to 2 additional TSs)
        mcs_to_write += random() % (TS_SIZE * 2);

        Log.trace() << "wait_for_data():"
                    << " min_mc_number=" << min_mc_number
                    << " _mc_written=" << _mc_written
                    << " mcs_to_write= " << mcs_to_write;

        while (mcs_to_write-- > 0) {
            int content_words = random() % (TYP_CNT_WORDS * 2);

            uint8_t hdrrev = 0x01;
            uint8_t sysid = 0x01;
            uint16_t flags = 0x0000;
            uint32_t size = (content_words + 2) * 8;
            uint16_t rsvd = 0x0000;
            uint64_t time = _mc_written;

            uint64_t hdr0 = (uint64_t) hdrrev << 56 | (uint64_t) sysid << 48
                            | (uint64_t) flags << 32 | (uint64_t) size;
            uint64_t hdr1 = (uint64_t) rsvd << 48 | (time & 0xFFFFFFFFFFFF);

            Log.trace() << "wait_for_data():"
                        << " _data_written=" << _data_written
                        << " _acked_data=" << _acked_data
                        << " content_words=" << content_words
                        << " DATA_WORDS=" << DATA_WORDS + 0;

            // check for space in data buffer
            if (_data_written - _acked_data + content_words + 2 > DATA_WORDS) {
                Log.warn() << "data buffer full";
                boost::this_thread::sleep(boost::posix_time::millisec(1));
                break;
            }

            // check for space in addr buffer
            if (_mc_written - _acked_mc == ADDR_WORDS) {
                Log.warn() << "addr buffer full";
                boost::this_thread::sleep(boost::posix_time::millisec(1));
                break;
            }

            // write to data buffer
            uint64_t start_addr = _data_written;
            _data[_data_written++ % DATA_WORDS] = hdr0;
            _data[_data_written++ % DATA_WORDS] = hdr1;
            for (int i = 0; i < content_words; i++) {
                //_data[_data_written++ % DATA_WORDS] = i << 16 | content_words;
                _data[_data_written++ % DATA_WORDS] = i + 0xA;
            }

            // write to addr buffer
            _addr[_mc_written++ % ADDR_WORDS] = start_addr;
        }
    };


    std::string
    getStateString() {
        std::ostringstream s;

        s << "/--- addr buf ---" << std::endl;
        s << "|";
        for (int i = 0; i < ADDR_WORDS; i++)
            s << " (" << i << ")" << _addr[i];
        s << std::endl;
        s << "| _mc_written = " << _mc_written << std::endl;
        s << "| _acked_mc = " << _acked_mc << std::endl;
        s << "/--- data buf ---" << std::endl;
        s << "|";
        for (int i = 0; i < DATA_WORDS; i++)
            s << " (" << i << ")"
              << std::hex << (_data[i] & 0xFFFF) << std::dec;
        s << std::endl;
        s << "| _data_written = " << _data_written << std::endl;
        s << "| _acked_data = " << _acked_data << std::endl;
        s << "\\---------";

        return s.str();
    }


    void post_send_data(uint64_t timeslice, int cn,
                        uint64_t mc_offset, uint64_t mc_length,
                        uint64_t data_offset, uint64_t data_length) {
        // initiate Infiniband transmission of TS
        int num_sge = 0;
        struct ibv_sge sge[4];
        // addr words
        if (mc_offset % ADDR_WORDS
                < (mc_offset + mc_length - 1) % ADDR_WORDS) {
            // one chunk
            sge[num_sge].addr =
                (uintptr_t) &_addr[mc_offset % ADDR_WORDS];
            sge[num_sge].length = sizeof(uint64_t) * mc_length;
            sge[num_sge++].lkey = _mr_addr->lkey;
        } else {
            // two chunks
            sge[num_sge].addr =
                (uintptr_t) &_addr[mc_offset % ADDR_WORDS];
            sge[num_sge].length =
                sizeof(uint64_t) * (ADDR_WORDS - mc_offset % ADDR_WORDS);
            sge[num_sge++].lkey = _mr_addr->lkey;
            sge[num_sge].addr = (uintptr_t) _addr;
            sge[num_sge].length =
                sizeof(uint64_t) * (mc_length - ADDR_WORDS
                                    + mc_offset % ADDR_WORDS);
            sge[num_sge++].lkey = _mr_addr->lkey;
        }
        // data words
        if (data_offset % DATA_WORDS
                < (data_offset + data_length - 1) % DATA_WORDS) {
            // one chunk
            sge[num_sge].addr =
                (uintptr_t) &_data[data_offset % DATA_WORDS];
            sge[num_sge].length = sizeof(uint64_t) * data_length;
            sge[num_sge++].lkey = _mr_data->lkey;
        } else {
            // two chunks
            sge[num_sge].addr =
                (uintptr_t) &_data[data_offset % DATA_WORDS];
            sge[num_sge].length =
                sizeof(uint64_t)
                * (DATA_WORDS - data_offset % DATA_WORDS);
            sge[num_sge++].lkey = _mr_data->lkey;
            sge[num_sge].addr = (uintptr_t) _data;
            sge[num_sge].length =
                sizeof(uint64_t) * (data_length - DATA_WORDS
                                    + data_offset % DATA_WORDS);
            sge[num_sge++].lkey = _mr_data->lkey;
        }

        _conn[cn]->post_send_data(sge, num_sge, timeslice, mc_length,
                                  data_length);
    }


    void
    sender_loop() {
        for (uint64_t timeslice = 0; timeslice < NUM_TS; timeslice++) {

            // wait until a complete TS is available in the input buffer
            uint64_t mc_offset = timeslice * TS_SIZE;
            uint64_t mc_length = TS_SIZE + TS_OVERLAP;
            while (_addr[(mc_offset + mc_length) % ADDR_WORDS] <= _acked_data)
                wait_for_data(mc_offset + mc_length + 1);

            uint64_t data_offset = _addr[mc_offset % ADDR_WORDS];
            uint64_t data_length = _addr[(mc_offset + mc_length) % ADDR_WORDS]
                                   - data_offset;

            Log.trace() << "SENDER working on TS " << timeslice
                        << ", MCs " << mc_offset << ".."
                        << (mc_offset + mc_length - 1)
                        << ", data words " << data_offset << ".."
                        << (data_offset + data_length - 1);
            Log.trace() << getStateString();

            int cn = target_cn_index(timeslice);

            _conn[cn]->waitForBufferSpace(data_length + mc_length, 1);

            post_send_data(timeslice, cn, mc_offset, mc_length,
                           data_offset, data_length);

            _conn[cn]->incWritePointers(data_length + mc_length, 1);

        }

        Log.info() << "SENDER loop done";
    }

    // TODO: split files: ibconn, cn-conn, inputbuffer

    virtual void onCompletion(struct ibv_wc& wc) {
        switch (wc.wr_id & 0xFF) {
        case ID_WRITE_DESC: {
            uint64_t ts = wc.wr_id >> 8;
            Log.debug() << "write completion for timeslice "
                        << ts;

            uint64_t acked_ts = _acked_mc / TS_SIZE;
            if (ts == acked_ts)
                do
                    acked_ts++;
                while (_ack[acked_ts % ACK_WORDS] > ts);
            else
                _ack[ts % ACK_WORDS] = ts;
            _acked_data =
                _addr[(acked_ts * TS_SIZE) % ADDR_WORDS];
            _acked_mc = acked_ts * TS_SIZE;
            Log.debug() << "new values: _acked_data="
                        << _acked_data
                        << " _acked_mc=" << _acked_mc;
        }
        break;

        case ID_RECEIVE_CN_ACK: {
            int cn = wc.wr_id >> 8;
            _conn[cn]->onCompleteRecv();
        }
        break;

        default:
            throw ApplicationException("wc for unknown wr_id");
        }
    };


private:

    uint64_t* _data;
    uint64_t* _addr;
    uint64_t* _ack;

    struct ibv_mr* _mr_data;
    struct ibv_mr* _mr_addr;

    // can be read by FLIB
    uint64_t _acked_mc;
    uint64_t _acked_data;

    // FLIB only
    uint64_t _mc_written;
    uint64_t _data_written;
};


#endif /* INPUTAPPLICATION_HPP */
