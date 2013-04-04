/**
 * \file ComputeBuffer.hpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef COMPUTEBUFFER_HPP
#define COMPUTEBUFFER_HPP

/// Compute buffer and input node connection container class.
/** A ComputeBuffer object represents a timeslice buffer (filled by
    the input nodes) and a group of timeslice building connections to
    input nodes. */

class ComputeBuffer : public IBConnectionGroup<ComputeNodeConnection>
{
    size_t _red_lantern = 0;
    uint64_t _completely_written = 0;
    uint64_t _acked = 0;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t, true> _ack;


public:
    concurrent_queue<TimesliceWorkItem> _work_items;
    concurrent_queue<TimesliceCompletion> _completions;

    /// The ComputeBuffer default constructor.
    ComputeBuffer() :
        _ack(par->cn_desc_buffer_size_exp())
    {
    }

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) {
        size_t in = wc.wr_id >> 8;
        assert(in < _conn.size());
        switch (wc.wr_id & 0xFF) {

        case ID_SEND_CN_ACK:
            if (out.beTrace()) {
                out.trace() << "[" << in << "] " << "COMPLETE SEND _send_cp_ack";
            }
            break;

        case ID_SEND_FINALIZE: {
            assert(_work_items.empty());
            assert(_completions.empty());
            _conn[in]->on_complete_send_finalize();
            _connections_done++;
            _all_done = (_connections_done == _conn.size());
            out.debug() << "SEND FINALIZE complete for id " << in << " all_done=" << _all_done;
            if (_all_done) {
                _work_items.stop();
                _completions.stop();
            }
        }
            break;

        case ID_RECEIVE_CN_WP: {
            _conn[in]->on_complete_recv();
            if (_connected == _conn.size() && in == _red_lantern) {
                auto new_red_lantern =
                    std::min_element(std::begin(_conn), std::end(_conn),
                                     [] (const std::unique_ptr<ComputeNodeConnection>& v1,
                                         const std::unique_ptr<ComputeNodeConnection>& v2)
                                     { return v1->cn_wp().desc < v2->cn_wp().desc; } );

                uint64_t new_completely_written = (*new_red_lantern)->cn_wp().desc;
                _red_lantern = std::distance(std::begin(_conn), new_red_lantern);

                for (uint64_t tpos = _completely_written; tpos < new_completely_written; tpos++) {
                    TimesliceWorkItem wi = {tpos};
                    _work_items.push(wi);
                }

                _completely_written = new_completely_written;
            }
        }
            break;

        default:
            throw InfinibandException("wc for unknown wr_id");
        }
    }

    virtual void handle_ts_completion() {
        try {
            while (true) {
                TimesliceCompletion c;
                _completions.wait_and_pop(c);
                if (c.ts_pos == _acked) {
                    do
                        _acked++;
                    while (_ack.at(_acked) > c.ts_pos);
                    for (auto& c : _conn)
                        c->inc_ack_pointers(_acked);
                } else
                    _ack.at(c.ts_pos) = c.ts_pos;
            }
        }
        catch (concurrent_queue<TimesliceCompletion>::Stopped) {
            out.trace() << "handle_ts_completion thread done";
        }
    }

    const RingBuffer<uint64_t>& data(int i) const {
        return _conn[i]->_data;
    }

    const RingBuffer<TimesliceComponentDescriptor>& desc(int i) const {
        return _conn[i]->_desc;
    }

};


#endif /* COMPUTEBUFFER_HPP */
