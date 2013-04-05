/**
 * \file InputBuffer.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef INPUTBUFFER_HPP
#define INPUTBUFFER_HPP

/// Input buffer and compute node connection container class.
/** An InputBuffer object represents an input buffer (filled by a
    FLIB) and a group of timeslice building connections to compute
    nodes. */

class InputBuffer : public IBConnectionGroup<InputNodeConnection>
{
public:

    /// The InputBuffer default constructor.
    InputBuffer() :
        _data(par->in_data_buffer_size_exp()),
        _addr(par->in_addr_buffer_size_exp()),
        _data_source(_data, _addr)
    {
        size_t min_ack_buffer_size = _addr.size() / par->timeslice_size() + 1;
        _ack.alloc_with_size(min_ack_buffer_size);

        VALGRIND_MAKE_MEM_DEFINED(_data.ptr(), _data.bytes());
        VALGRIND_MAKE_MEM_DEFINED(_addr.ptr(), _addr.bytes());
    }

    /// The InputBuffer default destructor.
    virtual ~InputBuffer() {
        if (_mr_addr) {
            ibv_dereg_mr(_mr_addr);
            _mr_addr = nullptr;
        }

        if (_mr_data) {
            ibv_dereg_mr(_mr_data);
            _mr_data = nullptr;
        }
    }

    /// The central loop for distributing timeslice data.
    void sender_loop() {
        for (uint64_t timeslice = 0; timeslice < par->max_timeslice_number();
             timeslice++) {

            // wait until a complete TS is available in the input buffer
            uint64_t mc_offset = timeslice * par->timeslice_size();
            uint64_t mc_length = par->timeslice_size() + par->overlap_size();
            
            _data_source.wait_for_data(mc_offset + mc_length + 1);
            
            uint64_t data_offset = _addr.at(mc_offset);
            uint64_t data_length = _addr.at(mc_offset + mc_length) - data_offset;

            if (out.beTrace()) {
                out.trace() << "SENDER working on TS " << timeslice
                            << ", MCs " << mc_offset << ".."
                            << (mc_offset + mc_length - 1)
                            << ", data words " << data_offset << ".."
                            << (data_offset + data_length - 1);
                out.trace() << get_state_string();
            }

            int cn = target_cn_index(timeslice);

            _conn[cn]->wait_for_buffer_space(data_length + mc_length, 1);

            post_send_data(timeslice, cn, mc_offset, mc_length,
                           data_offset, data_length);

            _conn[cn]->inc_write_pointers(data_length + mc_length, 1);
        }

        for (auto& c : _conn)
            c->finalize();

        out.debug() << "SENDER loop done";
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
    RingBuffer<uint64_t, true> _ack;

    /// Number of acknowledged MCs. Written to FLIB.
    uint64_t _acked_mc = 0;
    
    /// Number of acknowledged data words. Written to FLIB.
    uint64_t _acked_data = 0;
    
    /// Data source (e.g., FLIB).
    DummyFlib _data_source;
    
    /// Return target computation node for given timeslice.
    int target_cn_index(uint64_t timeslice) {
        return timeslice % _conn.size();
    }

    void dump_mr(struct ibv_mr* mr) {
        out.debug() << "ibv_mr dump:";
        out.debug() << " addr=" << (uint64_t) mr->addr;
        out.debug() << " length=" << (uint64_t) mr->length;
        out.debug() << " lkey=" << (uint64_t) mr->lkey;
        out.debug() << " rkey=" << (uint64_t) mr->rkey;
    }

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

            if (out.beDebug()) {
                dump_mr(_mr_addr);
                dump_mr(_mr_data);
            }
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
            uint64_t ts = wc.wr_id >> 24;

            int cn = (wc.wr_id >> 8) & 0xFFFF;
            _conn[cn]->on_complete_write();

            uint64_t acked_ts = _acked_mc / par->timeslice_size();
            if (ts == acked_ts)
                do
                    acked_ts++;
                while (_ack.at(acked_ts) > ts);
            else
                _ack.at(ts) = ts;
            _acked_data = _addr.at(acked_ts * par->timeslice_size());
            _acked_mc = acked_ts * par->timeslice_size();
            _data_source.update_ack_pointers(_acked_data, _acked_mc);
            if (out.beDebug())
                out.debug() << "write timeslice " << ts << " complete, now: _acked_data="
                            << _acked_data << " _acked_mc=" << _acked_mc;
        }
            break;

        case ID_RECEIVE_CN_ACK: {
            int cn = wc.wr_id >> 8;
            _conn[cn]->on_complete_recv();
            if (_conn[cn]->done()) {
                _connections_done++;
                _all_done = (_connections_done == _conn.size());
                out.debug() << "ID_RECEIVE_CN_ACK final for id " << cn << " all_done=" << _all_done;
            }
        }
            break;

        case ID_SEND_CN_WP: {
        }
            break;
            
        default:
            out.error() << "wc for unknown wr_id=" << (wc.wr_id & 0xFF);
            throw InfinibandException("wc for unknown wr_id");
        }
    }
};


#endif /* INPUTBUFFER_HPP */
