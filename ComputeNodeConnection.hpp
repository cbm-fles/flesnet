/**
 * \file ComputeNodeConnection.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef COMPUTENODECONNECTION_HPP
#define COMPUTENODECONNECTION_HPP

#include <boost/thread.hpp>
#include "Infiniband.hpp"
#include "log.hpp"


/// Compute node connection class.
/** A ComputeNodeConnection object represents the endpoint of a single
    timeslice building connection from an input node to a compute
    node. */

class ComputeNodeConnection : public IBConnection
{
public:

    /// The ComputeNodeConnection constructor.
    ComputeNodeConnection(struct rdma_event_channel* ec, int index) :
        IBConnection(ec, index), _our_turn(1) {
        memset(&_receive_cn_ack, 0, sizeof(cn_bufpos_t));
        memset(&_cn_ack, 0, sizeof(cn_bufpos_t));
        memset(&_cn_wp, 0, sizeof(cn_bufpos_t));
        memset(&_send_cn_wp, 0, sizeof(cn_bufpos_t));
    }

    /// Handle RDMA_CM_EVENT_ADDR_RESOLVED event for this connection.
    void onAddrResolved(struct ibv_pd* pd) {
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

        // setup send and receive buffers
        recv_sge.addr = (uintptr_t) &_receive_cn_ack;
        recv_sge.length = sizeof(cn_bufpos_t);
        recv_sge.lkey = _mr_recv->lkey;
        memset(&recv_wr, 0, sizeof recv_wr);
        recv_wr.wr_id = ID_RECEIVE_CN_ACK | (_index << 8);
        recv_wr.sg_list = &recv_sge;
        recv_wr.num_sge = 1;

        send_sge.addr = (uintptr_t) &_send_cn_wp;
        send_sge.length = sizeof(cn_bufpos_t);
        send_sge.lkey = _mr_send->lkey;
        memset(&send_wr, 0, sizeof send_wr);
        send_wr.wr_id = ID_SEND_CN_WP;
        send_wr.opcode = IBV_WR_SEND;
        send_wr.sg_list = &send_sge;
        send_wr.num_sge = 1;

        // post initial receive request
        postReceiveCnAck();
    }

    /// Handle Infiniband receive completion notification
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
        postReceiveCnAck();
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

    /// Post a receive work request (WR) to the receive queue
    void postReceiveCnAck() {
        Log.trace() << "[" << _index << "] "
                    << "POST RECEIVE _receive_cn_ack";
        if (ibv_post_recv(qp(), &recv_wr, &bad_recv_wr))
            throw ApplicationException("ibv_post_recv failed");
    }

    /// Post a send work request (WR) to the send queue
    void post_send_cn_wp() {
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


#endif /* COMPUTENODECONNECTION_HPP */
