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
    InputBuffer(uint64_t input_index,
                DataSource& data_source,
                const std::vector<std::string>& compute_hostnames,
                const std::vector<std::string>& compute_services,
                uint32_t timeslice_size,
                uint32_t overlap_size,
                uint32_t max_timeslice_number) :
        _input_index(input_index),
        _data_source(data_source),
        _compute_hostnames(compute_hostnames),
        _compute_services(compute_services),
        _timeslice_size(timeslice_size),
        _overlap_size(overlap_size),
        _max_timeslice_number(max_timeslice_number)
    {
        size_t min_ack_buffer_size = _data_source.desc_buffer().size() / _timeslice_size + 1;
        _ack.alloc_with_size(min_ack_buffer_size);

        VALGRIND_MAKE_MEM_DEFINED(_data_source.data_buffer().ptr(), _data_source.data_buffer().bytes());
        VALGRIND_MAKE_MEM_DEFINED(_data_source.desc_buffer().ptr(), _data_source.desc_buffer().bytes());
    }

    /// The InputBuffer default destructor.
    virtual ~InputBuffer() {
        if (_mr_desc) {
            ibv_dereg_mr(_mr_desc);
            _mr_desc = nullptr;
        }

        if (_mr_data) {
            ibv_dereg_mr(_mr_data);
            _mr_data = nullptr;
        }
    }

    virtual void run() {
        set_cpu(0);

        connect();
        handle_cm_events(_compute_hostnames.size());
        boost::thread t1(&InputBuffer::completion_handler, this);

        sender_loop();

        t1.join();
        boost::thread t2(&InputBuffer::handle_cm_events, this, 0);
        disconnect();
        t2.join();

        summary();
    }

    /// The central loop for distributing timeslice data.
    void sender_loop() {
        set_cpu(2);

        for (uint64_t timeslice = 0; timeslice < _max_timeslice_number;
             timeslice++) {

            // wait until a complete TS is available in the input buffer
            uint64_t mc_offset = timeslice * _timeslice_size;
            uint64_t mc_length = _timeslice_size + _overlap_size;
            
            _data_source.wait_for_data(mc_offset + mc_length + 1);
            
            uint64_t data_offset = _data_source.desc_buffer().at(mc_offset).offset;
            uint64_t data_length = _data_source.desc_buffer().at(mc_offset + mc_length).offset - data_offset;

            uint64_t total_length = data_length + mc_length * sizeof(MicrosliceDescriptor);

            if (out.beTrace()) {
                out.trace() << "SENDER working on TS " << timeslice
                            << ", MCs " << mc_offset << ".."
                            << (mc_offset + mc_length - 1)
                            << ", data bytes " << data_offset << ".."
                            << (data_offset + data_length - 1);
                out.trace() << get_state_string();
            }

            int cn = target_cn_index(timeslice);

            // number of bytes to skip in advance (to avoid buffer wrap)
            uint64_t skip = _conn[cn]->skip_required(total_length);
            total_length += skip;

            _conn[cn]->wait_for_buffer_space(total_length, 1);

            post_send_data(timeslice, cn, mc_offset, mc_length, data_offset, data_length, skip);

            _conn[cn]->inc_write_pointers(total_length, 1);
        }

        for (auto& c : _conn)
            c->finalize();

        out.debug() << "[i" << _input_index << "] " << "SENDER loop done";
    }

    std::unique_ptr<InputNodeConnection> create_input_node_connection(uint_fast16_t index)
    {
        unsigned int max_send_wr = 8000;

        // limit pending write requests so that send queue and completion queue do not overflow
        unsigned int max_pending_write_requests =
            std::min(static_cast<unsigned int>((max_send_wr - 1) / 3),
                     static_cast<unsigned int>((par->num_cqe() - 1) / _compute_hostnames.size()));

        std::unique_ptr<InputNodeConnection> connection
            (new InputNodeConnection(_ec, index, _input_index, max_send_wr,
                                     max_pending_write_requests));
        return connection;
    }

    /// Initiate connection requests to list of target hostnames.
    void connect() {
        for (unsigned int i = 0; i < _compute_hostnames.size(); i++) {
            std::unique_ptr<InputNodeConnection> connection = create_input_node_connection(i);
            connection->connect(_compute_hostnames[i], _compute_services[i]);
            _conn.push_back(std::move(connection));
        }
    };

private:
    uint64_t _input_index;

    /// Input data buffer. Filled by FLIB.
    RingBuffer<> _data;

    /// InfiniBand memory region descriptor for input data buffer.
    struct ibv_mr* _mr_data = nullptr;

    /// Input descriptor buffer. Filled by FLIB.
    RingBuffer<MicrosliceDescriptor> _desc;

    /// InfiniBand memory region descriptor for input descriptor buffer.
    struct ibv_mr* _mr_desc = nullptr;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t, true> _ack;

    /// Number of acknowledged MCs. Written to FLIB.
    uint64_t _acked_mc = 0;
    
    /// Number of acknowledged data bytes. Written to FLIB.
    uint64_t _acked_data = 0;
    
    /// Data source (e.g., FLIB).
    DataSource& _data_source;
    
    const std::vector<std::string>& _compute_hostnames;
    const std::vector<std::string>& _compute_services;

    const uint32_t _timeslice_size;
    const uint32_t _overlap_size;
    const uint32_t _max_timeslice_number;

    /// Return target computation node for given timeslice.
    int target_cn_index(uint64_t timeslice) {
        return timeslice % _conn.size();
    }

    void dump_mr(struct ibv_mr* mr) {
        out.debug() << "[i" << _input_index << "] " << "ibv_mr dump:";
        out.debug() << " addr=" << (uint64_t) mr->addr;
        out.debug() << " length=" << (uint64_t) mr->length;
        out.debug() << " lkey=" << (uint64_t) mr->lkey;
        out.debug() << " rkey=" << (uint64_t) mr->rkey;
    }

    virtual void on_addr_resolved(struct rdma_cm_id* id) {
        IBConnectionGroup<InputNodeConnection>::on_addr_resolved(id);

        if (!_mr_data) {
            // Register memory regions.
            _mr_data = ibv_reg_mr(_pd, _data_source.data_buffer().ptr(), _data_source.data_buffer().bytes(),
                                  IBV_ACCESS_LOCAL_WRITE);
            if (!_mr_data)
                throw InfinibandException
                    ("registration of memory region failed");

            _mr_desc = ibv_reg_mr(_pd, _data_source.desc_buffer().ptr(), _data_source.desc_buffer().bytes(),
                                  IBV_ACCESS_LOCAL_WRITE);
            if (!_mr_desc)
                throw InfinibandException
                    ("registration of memory region failed");

            if (out.beDebug()) {
                dump_mr(_mr_desc);
                dump_mr(_mr_data);
            }
        }
    }
    
    /// Handle RDMA_CM_REJECTED event.
    virtual void on_rejected(struct rdma_cm_event* event) {
        InputNodeConnection* conn = (InputNodeConnection*) event->id->context;

        conn->on_rejected(event);
        uint_fast16_t i = conn->index();
        _conn.at(i) = nullptr;

        // immediately initiate retry
        std::unique_ptr<InputNodeConnection> connection = create_input_node_connection(i);
        connection->connect(_compute_hostnames[i], _compute_services[i]);
        _conn.at(i) = std::move(connection);
    }

    /// Return string describing buffer contents, suitable for debug output.
    std::string get_state_string() {
        std::ostringstream s;

        s << "/--- desc buf ---" << std::endl;
        s << "|";
        for (unsigned int i = 0; i < _data_source.desc_buffer().size(); i++)
            s << " (" << i << ")" << _data_source.desc_buffer().at(i).offset;
        s << std::endl;
        s << "| _acked_mc = " << _acked_mc << std::endl;
        s << "/--- data buf ---" << std::endl;
        s << "|";
        for (unsigned int i = 0; i < _data_source.data_buffer().size(); i++)
            s << " (" << i << ")" << std::hex << _data_source.data_buffer().at(i) << std::dec;
        s << std::endl;
        s << "| _acked_data = " << _acked_data << std::endl;
        s << "\\---------";
        
        return s.str();
    }

    /// Create gather list for transmission of timeslice
    void post_send_data(uint64_t timeslice, int cn,
                        uint64_t mc_offset, uint64_t mc_length,
                        uint64_t data_offset, uint64_t data_length, uint64_t skip) {
        int num_sge = 0;
        struct ibv_sge sge[4];
        // descriptors
        if ((mc_offset & _data_source.desc_buffer().size_mask())
            < ((mc_offset + mc_length - 1) & _data_source.desc_buffer().size_mask())) {
            // one chunk
            sge[num_sge].addr = (uintptr_t) &_data_source.desc_buffer().at(mc_offset);
            sge[num_sge].length = sizeof(MicrosliceDescriptor) * mc_length;
            sge[num_sge++].lkey = _mr_desc->lkey;
        } else {
            // two chunks
            sge[num_sge].addr = (uintptr_t) &_data_source.desc_buffer().at(mc_offset);
            sge[num_sge].length =
              sizeof(MicrosliceDescriptor) * (_data_source.desc_buffer().size()
                                              - (mc_offset & _data_source.desc_buffer().size_mask()));
            sge[num_sge++].lkey = _mr_desc->lkey;
            sge[num_sge].addr = (uintptr_t) _data_source.desc_buffer().ptr();
            sge[num_sge].length =
              sizeof(MicrosliceDescriptor) * (mc_length - _data_source.desc_buffer().size()
                                              + (mc_offset & _data_source.desc_buffer().size_mask()));
            sge[num_sge++].lkey = _mr_desc->lkey;
        }
        // data
        if ((data_offset & _data_source.data_buffer().size_mask())
            < ((data_offset + data_length - 1) & _data_source.data_buffer().size_mask())) {
            // one chunk
            sge[num_sge].addr = (uintptr_t) &_data_source.data_buffer().at(data_offset);
            sge[num_sge].length = data_length;
            sge[num_sge++].lkey = _mr_data->lkey;
        } else {
            // two chunks
            sge[num_sge].addr = (uintptr_t) &_data_source.data_buffer().at(data_offset);
            sge[num_sge].length = _data_source.data_buffer().size() - (data_offset & _data_source.data_buffer().size_mask());
            sge[num_sge++].lkey = _mr_data->lkey;
            sge[num_sge].addr = (uintptr_t) _data_source.data_buffer().ptr();
            sge[num_sge].length = data_length - _data_source.data_buffer().size() + (data_offset & _data_source.data_buffer().size_mask());
            sge[num_sge++].lkey = _mr_data->lkey;
        }

        _conn[cn]->send_data(sge, num_sge, timeslice, mc_length, data_length, skip);
    }

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) {
        switch (wc.wr_id & 0xFF) {
        case ID_WRITE_DESC: {
            uint64_t ts = wc.wr_id >> 24;

            int cn = (wc.wr_id >> 8) & 0xFFFF;
            _conn[cn]->on_complete_write();

            uint64_t acked_ts = _acked_mc / _timeslice_size;
            if (ts == acked_ts)
                do
                    acked_ts++;
                while (_ack.at(acked_ts) > ts);
            else
                _ack.at(ts) = ts;
            _acked_data = _data_source.desc_buffer().at(acked_ts * _timeslice_size).offset;
            _acked_mc = acked_ts * _timeslice_size;
            _data_source.update_ack_pointers(_acked_data, _acked_mc);
            if (out.beDebug())
                out.debug() << "[i" << _input_index << "] "
                            << "write timeslice " << ts << " complete, now: _acked_data="
                            << _acked_data << " _acked_mc=" << _acked_mc;
        }
            break;

        case ID_RECEIVE_CN_ACK: {
            int cn = wc.wr_id >> 8;
            _conn[cn]->on_complete_recv();
            if (_conn[cn]->done()) {
                _connections_done++;
                _all_done = (_connections_done == _conn.size());
                out.debug() << "[i" << _input_index << "] "
                            << "ID_RECEIVE_CN_ACK final for id " << cn << " all_done=" << _all_done;
            }
        }
            break;

        case ID_SEND_CN_WP: {
        }
            break;
            
        default:
            out.error() << "[i" << _input_index << "] "
                        << "wc for unknown wr_id=" << (wc.wr_id & 0xFF);
            throw InfinibandException("wc for unknown wr_id");
        }
    }
};


#endif /* INPUTBUFFER_HPP */
