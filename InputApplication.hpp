/*
 * InputApplication.hpp
 *
 * 2012, Jan de Cuveland
 */

#ifndef INPUTAPPLICATION_HPP
#define INPUTAPPLICATION_HPP

#include <boost/thread.hpp>
#include "log.hpp"


class InputContext {
public:
    // struct ibv_comp_channel *channel;
    // struct ibv_pd           *pd;
    // struct ibv_mr           *mr;
    // struct ibv_cq           *cq;
    // struct ibv_qp           *qp;
    // void                    *buf;
    Application::pdata_t _server_pdata[2];
    struct ibv_pd* _pd;
    struct ibv_comp_channel* _comp_chan;
    struct ibv_qp* _qp;
    struct ibv_cq* _cq;

    InputContext(Parameters const& par) : _par(par) { };

    void connect() {
        const char* hostname = _par.compute_nodes().at(0).c_str();

        Log.debug() << "setting up RDMA CM structures";

        // Create an rdma event channel
        struct rdma_event_channel* cm_channel = rdma_create_event_channel();
        if (!cm_channel)
            throw ApplicationException("event channel creation failed");

        // Create rdma id (for listening)
        struct rdma_cm_id* cm_id;
        int err = rdma_create_id(cm_channel, &cm_id, NULL, RDMA_PS_TCP);
        if (err)
            throw ApplicationException("id creation failed");

        // Retrieve a list of IP addresses and port numbers
        // for given hostname and service
        struct addrinfo hints;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* res;

        char* service;
        if (asprintf(&service, "%d", BASE_PORT) < 0)
            throw ApplicationException("invalid port");

        err = getaddrinfo(hostname, service, &hints, &res);
        if (err)
            throw ApplicationException("getaddrinfo failed");

        Log.debug() << "resolution of server address and route";

        // Resolve destination address from IP address to rdma address
        // (binds cm_id to a local device)
        for (struct addrinfo* t = res; t; t = t->ai_next) {
            err = rdma_resolve_addr(cm_id, NULL, t->ai_addr,
                                    RESOLVE_TIMEOUT_MS);
            if (!err)
                break;
        }
        if (err)
            throw ApplicationException("rdma_resolve_addr failed");

        freeaddrinfo(res);
        free(service);

        // Retrieve the next pending communication event
        struct rdma_cm_event* event;
        err = rdma_get_cm_event(cm_channel, &event);
        if (err)
            throw ApplicationException("rdma_get_cm_event failed");

        // Assert that event is address resolution completion
        if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED)
            throw ApplicationException("RDMA address resolution failed");

        // Free the communication event
        rdma_ack_cm_event(event);

        // Resolv an rdma route to the dest address to establish a connection
        err = rdma_resolve_route(cm_id, RESOLVE_TIMEOUT_MS);
        if (err)
            throw ApplicationException("rdma_resolve_route failed");

        // Retrieve the next pending communication event
        err = rdma_get_cm_event(cm_channel, &event);
        if (err)
            throw ApplicationException("rdma_get_cm_event failed");

        // Assert that event is route resolution completion
        if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED)
            throw ApplicationException("RDMA route resolution failed");

        // Free the communication event
        rdma_ack_cm_event(event);

        Log.debug() << "creating verbs objects";

        // Allocate a protection domain (PD) for the given context
        _pd = ibv_alloc_pd(cm_id->verbs);
        if (!_pd)
            throw ApplicationException("ibv_alloc_pd failed");

        // Create a completion event channel for the given context
        _comp_chan = ibv_create_comp_channel(cm_id->verbs);
        if (!_comp_chan)
            throw ApplicationException("ibv_create_comp_channel failed");

        // Create a completion queue (CQ) for the given context with at least 40
        // entries, using the given completion channel to return completion events
        _cq = ibv_create_cq(cm_id->verbs, 40, NULL, _comp_chan, 0);
        if (!_cq)
            throw ApplicationException("ibv_create_cq failed");

        // Request a completion notification on the given completion queue
        // ("one shot" - only one completion event will be generated)
        if (ibv_req_notify_cq(_cq, 0))
            throw ApplicationException("ibv_req_notify_cq failed");

        // Allocate a queue pair (QP) associated with the specified rdma id
        struct ibv_qp_init_attr qp_attr;
        memset(&qp_attr, 0, sizeof qp_attr);
        qp_attr.cap.max_send_wr = 20; // max num of outstanding WRs in the SQ
        qp_attr.cap.max_send_sge = 8; // max num of outstanding scatter/gather
        // elements in a WR in the SQ
        qp_attr.cap.max_recv_wr = 20;  // max num of outstanding WRs in the RQ
        qp_attr.cap.max_recv_sge = 8; // max num of outstanding scatter/gather
        // elements in a WR in the RQ
        qp_attr.cap.max_inline_data = sizeof(tscdesc_t) * 10;
        qp_attr.send_cq = _cq;
        qp_attr.recv_cq = _cq;
        qp_attr.qp_type = IBV_QPT_RC; // reliable connection
        err = rdma_create_qp(cm_id, _pd, &qp_attr);
        if (err)
            throw ApplicationException("creation of QP failed");

        Log.debug() << "connect to server";

        // Initiate an active connection request
        struct rdma_conn_param conn_param;
        memset(&conn_param, 0, sizeof conn_param);
        conn_param.initiator_depth = 1;
        conn_param.retry_count = 7;
        err = rdma_connect(cm_id, &conn_param);
        if (err)
            throw ApplicationException("rdma_connect failed");

        // Retrieve next pending communication event on the given channel (BLOCKING)
        err = rdma_get_cm_event(cm_channel, &event);
        if (err)
            throw ApplicationException("retrieval of communication event failed");

        // Assert that a connection has been established with the remote end point
        if (event->event != RDMA_CM_EVENT_ESTABLISHED)
            throw ApplicationException("connection could not be established");

        // Copy server private data from event
        memcpy(&_server_pdata, event->param.conn.private_data,
               sizeof _server_pdata);

        // Free the communication event
        rdma_ack_cm_event(event);

        _qp = cm_id->qp;

        Log.info() << "connection established";
    }

private:
    Parameters const& _par;
};


class InputBuffer {
public:
    InputBuffer(InputContext* ctx) :
        _our_turn(1),
        _acked_mc(0), _acked_data(0),
        _mc_written(0), _data_written(0) {
        _data = new uint64_t[DATA_WORDS]();
        _addr = new uint64_t[ADDR_WORDS]();
        _ack = new uint64_t[ACK_WORDS]();
        memset(&_receive_cn_ack, 0, sizeof(cn_bufpos_t));
        memset(&_cn_ack, 0, sizeof(cn_bufpos_t));
        memset(&_cn_wp, 0, sizeof(cn_bufpos_t));
        memset(&_send_cn_wp, 0, sizeof(cn_bufpos_t));
        _ctx = ctx;
    };

    ~InputBuffer() {
        delete [] _data;
        delete [] _addr;
        delete [] _ack;
    };

    void sender_thread() {
        sender_loop();
    };
    void completion_thread() {
        completion_handler();
    };

    void
    setup() {
        // register memory regions
        _mr_recv = ibv_reg_mr(_ctx->_pd, &_receive_cn_ack,
                              sizeof(cn_bufpos_t),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_recv)
            throw ApplicationException("registration of memory region failed");

        _mr_send = ibv_reg_mr(_ctx->_pd, &_send_cn_wp,
                              sizeof(cn_bufpos_t), 0);
        if (!_mr_send)
            throw ApplicationException("registration of memory region failed");

        _mr_data = ibv_reg_mr(_ctx->_pd, _data,
                              DATA_WORDS * sizeof(uint64_t),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_data)
            throw ApplicationException("registration of memory region failed");

        _mr_addr = ibv_reg_mr(_ctx->_pd, _addr,
                              ADDR_WORDS * sizeof(uint64_t),
                              IBV_ACCESS_LOCAL_WRITE);
        if (!_mr_addr)
            throw ApplicationException("registration of memory region failed");
    }

private:
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
                Log.error() << "data buffer full";
                boost::this_thread::sleep(boost::posix_time::millisec(1));
                break;
            }

            // check for space in addr buffer
            if (_mc_written - _acked_mc == ADDR_WORDS) {
                Log.error() << "addr buffer full";
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


    int my_post_send(struct ibv_qp* qp, struct ibv_send_wr* wr,
                     struct ibv_send_wr** bad_wr) {
        struct ibv_send_wr* wr_first = wr;

        struct bufdesc {
            uint64_t addr;
            size_t nmemb;
            size_t size;
            char* name;
        };

        struct bufdesc source_desc[] = {
            {(uint64_t) _data, DATA_WORDS, sizeof(uint64_t), (char*) "data"},
            {(uint64_t) _addr, ADDR_WORDS, sizeof(uint64_t), (char*) "addr"},
            {0, 0, 0, 0}
        };

        struct bufdesc target_desc[] = {
            {
                _ctx->_server_pdata[0].buf_va, CN_DATABUF_WORDS,
                sizeof(uint64_t), (char*) "cn_data"
            },
            {
                _ctx->_server_pdata[1].buf_va, CN_DESCBUF_WORDS,
                sizeof(tscdesc_t), (char*) "cn_desc"
            },
            {0, 0, 0, 0}
        };

        if (Log.beTrace()) {
            std::ostringstream s;

            s << "/--- ibv_post_send() ---" << std::endl;
            int wr_num = 0;
            while (wr) {
                s << "| wr" << wr_num << ": id=" << wr->wr_id
                  << " opcode=" << wr->opcode
                  << " num_sge=" << wr->num_sge;
                if (wr->wr.rdma.remote_addr) {
                    uint64_t addr = wr->wr.rdma.remote_addr;
                    s << " rdma.remote_addr=";
                    struct bufdesc* b = target_desc;
                    while (b->name) {
                        if (addr >= b->addr
                                && addr < b->addr + b->nmemb * b->size) {
                            s << b->name << "["
                              << (addr - b->addr) / b->size << "]";
                            break;
                        }
                        b++;
                    }
                    if (!b->name)
                        s << addr;
                }
                s << std::endl;
                s << "|   sg_list=";
                uint32_t total_length = 0;
                for (int i = 0; i < wr->num_sge; i++) {
                    uint64_t addr = wr->sg_list[i].addr;
                    uint32_t length =  wr->sg_list[i].length;
                    struct bufdesc* b = source_desc;
                    while (b->name) {
                        if (addr >= b->addr
                                && addr < b->addr + b->nmemb * b->size) {
                            s << b->name << "["
                              << (addr - b->addr) / b->size << "]:";
                            break;
                        }
                        b++;
                    }
                    s << length / (sizeof(uint64_t)) << " ";
                    total_length += length;
                }
                s << "(" << total_length / (sizeof(uint64_t))
                  << " words total)" << std::endl;
                wr = wr->next;
                wr_num++;
            }
            s << "\\---------";
            Log.trace() << s.str();
        }

        return ibv_post_send(qp, wr_first, bad_wr);
    }


    void post_send_data(uint64_t timeslice,
                        uint64_t mc_offset, uint64_t mc_length,
                        uint64_t data_offset, uint64_t data_length) {
        // initiate Infiniband transmission of TS
        int num_sge = 0;
        struct ibv_sge sge[4];
        int num_sge2 = 0;
        struct ibv_sge sge2[4];
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

        uint64_t target_words_left =
            CN_DATABUF_WORDS - _cn_wp.data % CN_DATABUF_WORDS;

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
        send_wr_ts.wr.rdma.rkey = _ctx->_server_pdata[0].buf_rkey;
        send_wr_ts.wr.rdma.remote_addr =
            (uintptr_t)(_ctx->_server_pdata[0].buf_va +
                        (_cn_wp.data % CN_DATABUF_WORDS) * sizeof(uint64_t));

        if (num_sge2) {
            memset(&send_wr_tswrap, 0, sizeof(send_wr_ts));
            send_wr_tswrap.wr_id = ID_WRITE_DATA_WRAP;
            send_wr_tswrap.opcode = IBV_WR_RDMA_WRITE;
            send_wr_tswrap.sg_list = sge2;
            send_wr_tswrap.num_sge = num_sge2;
            send_wr_tswrap.wr.rdma.rkey = _ctx->_server_pdata[0].buf_rkey;
            send_wr_tswrap.wr.rdma.remote_addr =
                (uintptr_t) _ctx->_server_pdata[0].buf_va;
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
        send_wr_tscdesc.wr.rdma.rkey = _ctx->_server_pdata[1].buf_rkey;
        send_wr_tscdesc.wr.rdma.remote_addr =
            (uintptr_t)(_ctx->_server_pdata[1].buf_va
                        + (_cn_wp.desc % CN_DESCBUF_WORDS)
                        * sizeof(tscdesc_t));

        Log.debug() << "post_send(timeslice " << timeslice << ")";

        // send everything
        struct ibv_send_wr* bad_send_wr;
        if (my_post_send(_ctx->_qp, &send_wr_ts, &bad_send_wr))
            throw ApplicationException("ibv_post_send failed");
    }


    void
    sender_loop() {
        setup_recv();
        setup_send();
        post_recv_cn_ack();

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

            // wait until enough space is available at target compute node
            {
                boost::mutex::scoped_lock lock(_cn_ack_mutex);
                Log.trace() << "SENDER data space (words) required="
                            << data_length + mc_length
                            << ", avail="
                            << _cn_ack.data + CN_DATABUF_WORDS - _cn_wp.data;
                Log.trace() << "SENDER desc space (words) required=" << 1
                            << ", avail="
                            << _cn_ack.desc + CN_DESCBUF_WORDS - _cn_wp.desc;
                while (_cn_ack.data - _cn_wp.data + CN_DATABUF_WORDS
                        < data_length + mc_length
                        ||
                        _cn_ack.desc - _cn_wp.desc + CN_DESCBUF_WORDS
                        < 1) {
                    {
                        boost::mutex::scoped_lock lock2(_cn_wp_mutex);
                        if (_our_turn) {
                            // send phony update to receive new pointers
                            {
                                Log.info() << "SENDER send phony update";
                                _our_turn = 0;
                                _send_cn_wp = _cn_wp;
                                post_send_cn_wp();
                            }
                        }
                    }
                    _cn_ack_cond.wait(lock);
                    Log.trace() << "SENDER (next try) space avail="
                                << _cn_ack.data - _cn_wp.data + CN_DATABUF_WORDS
                                << " desc_avail="
                                << _cn_ack.desc - _cn_wp.desc
                                + CN_DESCBUF_WORDS;
                }
            }

            post_send_data(timeslice, mc_offset, mc_length,
                           data_offset, data_length);

            {
                boost::mutex::scoped_lock lock(_cn_wp_mutex);
                _cn_wp.data += data_length + mc_length;
                _cn_wp.desc++;
                if (_our_turn) {
                    _our_turn = 0;
                    _send_cn_wp = _cn_wp;
                    post_send_cn_wp();
                }
            }
        }

        Log.info() << "SENDER loop done";
    }

    struct ibv_sge recv_sge;
    struct ibv_recv_wr recv_wr;
    struct ibv_recv_wr* bad_recv_wr;

    struct ibv_sge send_sge;
    struct ibv_send_wr send_wr;
    struct ibv_send_wr* bad_send_wr;

    void setup_recv() {
        recv_sge.addr = (uintptr_t) &_receive_cn_ack;
        recv_sge.length = sizeof(cn_bufpos_t);
        recv_sge.lkey = _mr_recv->lkey;
        memset(&_receive_cn_ack, 0, sizeof _receive_cn_ack);
        recv_wr.wr_id = ID_RECEIVE_CN_ACK;
        recv_wr.sg_list = &recv_sge;
        recv_wr.num_sge = 1;
    }

    void setup_send() {
        send_sge.addr = (uintptr_t) &_send_cn_wp;
        send_sge.length = sizeof(cn_bufpos_t);
        send_sge.lkey = _mr_send->lkey;
        memset(&_send_cn_wp, 0, sizeof _send_cn_wp);
        send_wr.wr_id = ID_SEND_CN_WP;
        send_wr.opcode = IBV_WR_SEND;
        send_wr.sg_list = &send_sge;
        send_wr.num_sge = 1;
    }

    void post_recv_cn_ack() {
        // Post a receive work request (WR) to the receive queue
        Log.trace() << "POST RECEIVE _receive_cn_ack";
        if (ibv_post_recv(_ctx->_qp, &recv_wr, &bad_recv_wr))
            throw ApplicationException("post_recv failed");
    }

    void post_send_cn_wp() {
        // Post a send work request (WR) to the send queue
        Log.trace() << "POST SEND _send_cp_wp (data=" << _send_cn_wp.data
                    << " desc=" << _send_cn_wp.desc << ")";
        if (my_post_send(_ctx->_qp, &send_wr, &bad_send_wr))
            throw ApplicationException("ibv_post_send(cn_wp) failed");
    }


    void
    completion_handler() {
        const int ne_max = 10;

        struct ibv_cq* ev_cq;
        void* ev_ctx;
        struct ibv_wc wc[ne_max];
        int ne;

        while (true) {
            if (ibv_get_cq_event(_ctx->_comp_chan, &ev_cq, &ev_ctx))
                throw ApplicationException("ibv_get_cq_event failed");

            ibv_ack_cq_events(ev_cq, 1);

            if (ev_cq != _ctx->_cq)
                throw ApplicationException("CQ event for unknown CQ");

            if (ibv_req_notify_cq(_ctx->_cq, 0))
                throw ApplicationException("ibv_req_notify_cq failed");

            while ((ne = ibv_poll_cq(_ctx->_cq, ne_max, wc))) {
                if (ne < 0)
                    throw ApplicationException("ibv_poll_cq failed");

                for (int i = 0; i < ne; i++) {
                    if (wc[i].status != IBV_WC_SUCCESS) {
                        std::ostringstream s;
                        s << ibv_wc_status_str(wc[i].status)
                          << " for wr_id " << (int) wc[i].wr_id;
                        throw ApplicationException(s.str());
                    }

                    switch (wc[i].wr_id & 0xFF) {
                    case ID_WRITE_DESC: {
                        uint64_t ts = wc[i].wr_id >> 8;
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

                    case ID_RECEIVE_CN_ACK:
                        Log.debug()
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
                        break;

                    default:
                        throw ApplicationException("wc for unknown wr_id");
                    }
                }
            }
        }
    }


private:

    struct ibv_mr* _mr_recv;
    struct ibv_mr* _mr_send;
    struct ibv_mr* _mr_data;
    struct ibv_mr* _mr_addr;

    InputContext* _ctx;

    int _our_turn;

    cn_bufpos_t _receive_cn_ack;
    cn_bufpos_t _cn_ack;

    cn_bufpos_t _cn_wp;
    cn_bufpos_t _send_cn_wp;

    uint64_t* _data;
    uint64_t* _addr;
    uint64_t* _ack;

    boost::mutex _cn_ack_mutex;
    boost::mutex _cn_wp_mutex;
    boost::condition_variable_any _cn_ack_cond;

    // can be read by FLIB
    uint64_t _acked_mc;
    uint64_t _acked_data;

    // FLIB only
    uint64_t _mc_written;
    uint64_t _data_written;
};


#endif /* INPUTAPPLICATION_HPP */
