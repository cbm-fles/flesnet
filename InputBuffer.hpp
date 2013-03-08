/**
 * \file InputBuffer.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef INPUTBUFFER_HPP
#define INPUTBUFFER_HPP

#include <cassert>
#include "InputNodeConnection.hpp"
#include "DataSource.hpp"
#include "global.hpp"


/// Input buffer and compute node connection container class.
/** An InputBuffer object represents an input buffer (filled by a
    FLIB) and a group of timeslice building connections to compute
    nodes. */

class InputBuffer : public IBConnectionGroup<InputNodeConnection>
{
public:

    /// The InputBuffer default constructor.
    InputBuffer() :
        _data(Par->in_data_buffer_size_exp()),
        _addr(Par->in_addr_buffer_size_exp()),
        _data_source(_data, _addr)
    {
        size_t min_ack_buffer_size = _addr.size() / Par->timeslice_size() + 1;
        _ack.alloc_with_size(min_ack_buffer_size);
    }

    /// The InputBuffer default destructor.
    virtual ~InputBuffer() {
        if (_mr_addr) {
            ibv_dereg_mr(_mr_addr);
            _mr_addr = 0;
        }

        if (_mr_data) {
            ibv_dereg_mr(_mr_data);
            _mr_data = 0;
        }
    }

    /// The central loop for distributing timeslice data.
    void sender_loop() {
        for (uint64_t timeslice = 0; timeslice < Par->max_timeslice_number();
             timeslice++) {

            // wait until a complete TS is available in the input buffer
            uint64_t mc_offset = timeslice * Par->timeslice_size();
            uint64_t mc_length = Par->timeslice_size() + Par->overlap_size();
            
            if (_addr.at(mc_offset + mc_length) <= _acked_data)
                _data_source.wait_for_data(mc_offset + mc_length + 1);
            assert(_addr.at(mc_offset + mc_length) > _acked_data);
            
            uint64_t data_offset = _addr.at(mc_offset);
            uint64_t data_length = _addr.at(mc_offset + mc_length)
                - data_offset;

            if (Log.beTrace()) {
                Log.trace() << "SENDER working on TS " << timeslice
                            << ", MCs " << mc_offset << ".."
                            << (mc_offset + mc_length - 1)
                            << ", data words " << data_offset << ".."
                            << (data_offset + data_length - 1);
                Log.trace() << get_state_string();
            }

            int cn = target_cn_index(timeslice);

            _conn[cn]->wait_for_buffer_space(data_length + mc_length, 1);

            post_send_data(timeslice, cn, mc_offset, mc_length,
                           data_offset, data_length);

            _conn[cn]->inc_write_pointers(data_length + mc_length, 1);
        }

        for(auto it = _conn.begin(); it != _conn.end(); ++it)
            (*it)->finalize();

        Log.info() << "SENDER loop done";
    }

    /// Handle RDMA_CM_EVENT_DISCONNECTED event.
    virtual void on_disconnect(struct rdma_cm_id* id) {
        InputNodeConnection* conn = (InputNodeConnection*) id->context;

        _aggregate_content_bytes_sent += conn->content_bytes_sent();
        _aggregate_bytes_sent += conn->total_bytes_sent();
        _aggregate_send_requests += conn->total_send_requests();
        _aggregate_recv_requests += conn->total_recv_requests();        

        IBConnectionGroup<InputNodeConnection>::on_disconnect(id);
    }

    /// Retrieve the number of bytes transmitted (without pointer updates).
    uint64_t aggregate_content_bytes_sent() const {
        return _aggregate_content_bytes_sent;
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


private:

    /// Input data buffer. Filled by FLIB.
    RingBuffer<uint64_t> _data;

    /// InfiniBand memory region descriptor for input data buffer.
    struct ibv_mr* _mr_data = nullptr;

    /// Input address buffer. Filled by FLIB.
    RingBuffer<uint64_t> _addr;

    /// InfiniBand memory region descriptor for input address buffer.
    struct ibv_mr* _mr_addr = nullptr;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t> _ack;

    /// Number of acknowledged MCs. Written to FLIB.
    uint64_t _acked_mc = 0;
    
    /// Number of acknowledged data words. Written to FLIB.
    uint64_t _acked_data = 0;
    
    /// Flag indicating completion of the sender loop for this run.
    bool _sender_loop_done;

    /// Number of connections in the done state.
    unsigned int _connections_done = 0;

    /// Total number of bytes transmitted (without pointer updates).
    uint64_t _aggregate_content_bytes_sent = 0;

    /// Total number of bytes transmitted.
    uint64_t _aggregate_bytes_sent = 0;

    /// Total number of SEND work requests.
    uint64_t _aggregate_send_requests = 0;

    /// Total number of RECV work requests.
    uint64_t _aggregate_recv_requests = 0;

    /// Data source (e.g., FLIB).
    DummyFlib _data_source;
    
    /// Return target computation node for given timeslice.
    int target_cn_index(uint64_t timeslice) {
        return timeslice % _conn.size();
    }

    /// Handle RDMA_CM_EVENT_ADDR_RESOLVED event.
    virtual void on_addr_resolved(struct rdma_cm_id* id) {
        IBConnectionGroup<InputNodeConnection>::on_addr_resolved(id);

        if (!_mr_data) {
            // Register memory regions.
            _mr_data = ibv_reg_mr(_pd, _data.ptr(), _data.bytes(),
                                  IBV_ACCESS_LOCAL_WRITE);
            if (!_mr_data)
                throw InfinibandException
                    ("registration of memory region failed");

            _mr_addr = ibv_reg_mr(_pd, _addr.ptr(), _addr.bytes(),
                                  IBV_ACCESS_LOCAL_WRITE);
            if (!_mr_addr)
                throw InfinibandException
                    ("registration of memory region failed");
        }
    }
    
    /// Return string describing buffer contents, suitable for debug output.
    std::string get_state_string() {
        std::ostringstream s;

        s << "/--- addr buf ---" << std::endl;
        s << "|";
        for (unsigned int i = 0; i < _addr.size(); i++)
            s << " (" << i << ")" << _addr.at(i);
        s << std::endl;
        s << "| _acked_mc = " << _acked_mc << std::endl;
        s << "/--- data buf ---" << std::endl;
        s << "|";
        for (unsigned int i = 0; i < _data.size(); i++)
            s << " (" << i << ")"
              << std::hex << (_data.at(i) & 0xFFFF) << std::dec;
        s << std::endl;
        s << "| _acked_data = " << _acked_data << std::endl;
        s << "\\---------";
        
        return s.str();
    }

    /// Create gather list for transmission of timeslice
    void post_send_data(uint64_t timeslice, int cn,
                        uint64_t mc_offset, uint64_t mc_length,
                        uint64_t data_offset, uint64_t data_length) {
        int num_sge = 0;
        struct ibv_sge sge[4];
        // addr words
        if ((mc_offset & _addr.size_mask())
            < ((mc_offset + mc_length - 1) & _addr.size_mask())) {
            // one chunk
            sge[num_sge].addr = (uintptr_t) &_addr.at(mc_offset);
            sge[num_sge].length = sizeof(uint64_t) * mc_length;
            sge[num_sge++].lkey = _mr_addr->lkey;
        } else {
            // two chunks
            sge[num_sge].addr = (uintptr_t) &_addr.at(mc_offset);
            sge[num_sge].length =
                sizeof(uint64_t) * (_addr.size()
                                    - (mc_offset & _addr.size_mask()));
            sge[num_sge++].lkey = _mr_addr->lkey;
            sge[num_sge].addr = (uintptr_t) _addr.ptr();
            sge[num_sge].length =
                sizeof(uint64_t) * (mc_length - _addr.size()
                                    + (mc_offset & _addr.size_mask()));
            sge[num_sge++].lkey = _mr_addr->lkey;
        }
        // data words
        if ((data_offset & _data.size_mask())
            < ((data_offset + data_length - 1) & _data.size_mask())) {
            // one chunk
            sge[num_sge].addr = (uintptr_t) &_data.at(data_offset);
            sge[num_sge].length = sizeof(uint64_t) * data_length;
            sge[num_sge++].lkey = _mr_data->lkey;
        } else {
            // two chunks
            sge[num_sge].addr = (uintptr_t) &_data.at(data_offset);
            sge[num_sge].length =
                sizeof(uint64_t)
                * (_data.size() - (data_offset & _data.size_mask()));
            sge[num_sge++].lkey = _mr_data->lkey;
            sge[num_sge].addr = (uintptr_t) _data.ptr();
            sge[num_sge].length =
                sizeof(uint64_t) * (data_length - _data.size()
                                    + (data_offset & _data.size_mask()));
            sge[num_sge++].lkey = _mr_data->lkey;
        }

        _conn[cn]->send_data(sge, num_sge, timeslice, mc_length, data_length);
    }

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) {
        switch (wc.wr_id & 0xFF) {
        case ID_WRITE_DESC: {
            uint64_t ts = wc.wr_id >> 8;
            Log.debug() << "write completion for timeslice " << ts;

            uint64_t acked_ts = _acked_mc / Par->timeslice_size();
            if (ts == acked_ts)
                do
                    acked_ts++;
                while (_ack.at(acked_ts) > ts);
            else
                _ack.at(ts) = ts;
            _acked_data = _addr.at(acked_ts * Par->timeslice_size());
            _acked_mc = acked_ts * Par->timeslice_size();
            _data_source.update_ack_pointers(_acked_data, _acked_mc);
            Log.debug() << "new values: _acked_data=" << _acked_data
                        << " _acked_mc=" << _acked_mc;
        }
            break;

        case ID_RECEIVE_CN_ACK: {
            int cn = wc.wr_id >> 8;
            _conn[cn]->on_complete_recv();
            if (_conn[cn]->done()) {
                _connections_done++;
                _all_done = (_connections_done == _conn.size());
                Log.debug() << "ID_RECEIVE_CN_ACK final for id " << cn << " alldone=" << _all_done;
            }
        }
            break;
            
        default:
            throw InfinibandException("wc for unknown wr_id");
        }
    }
};


#endif /* INPUTBUFFER_HPP */
