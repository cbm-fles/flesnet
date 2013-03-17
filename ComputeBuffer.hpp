/**
 * \file ComputeBuffer.hpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef COMPUTEBUFFER_HPP
#define COMPUTEBUFFER_HPP

#include <algorithm>
#include "ComputeNodeConnection.hpp"


/// Compute buffer and input node connection container class.
/** A ComputeBuffer object represents a timeslice buffer (filled by
    the input nodes) and a group of timeslice building connections to
    input nodes. */

class ComputeBuffer : public IBConnectionGroup<ComputeNodeConnection>
{
    size_t _red_lantern = 0;
    uint64_t _completely_written = 0;

public:
    
    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) {
        switch (wc.wr_id & 0xFF) {
        case ID_SEND_CN_ACK:
            out.debug() << "SEND complete";
            break;

        case ID_SEND_FINALIZE: {
            int in = wc.wr_id >> 8;
            _conn[in]->on_complete_send_finalize();
            _connections_done++;
            _all_done = (_connections_done == _conn.size());
            out.info() << "SEND FINALIZE complete for id " << in << " alldone=" << _all_done;
        }
            break;

        case ID_RECEIVE_CN_WP: {
            size_t in = wc.wr_id >> 8;
            _conn[in]->on_complete_recv();
            if (in == _red_lantern) {
                auto new_red_lantern =
                    std::min_element(std::begin(_conn), std::end(_conn),
                                     [] (const std::unique_ptr<ComputeNodeConnection>& v1,
                                         const std::unique_ptr<ComputeNodeConnection> &v2)
                                     { return v1->cn_wp().desc < v2->cn_wp().desc; } );

                uint64_t new_completely_written = (*new_red_lantern)->cn_wp().desc;
                _red_lantern = std::distance(std::begin(_conn), new_red_lantern);

                for (uint64_t tpos = _completely_written; tpos < new_completely_written; tpos++)
                    submit_timeslice(tpos);

                _completely_written = new_completely_written;
            }
        }
            break;

        default:
            throw InfinibandException("wc for unknown wr_id");
        }
    }

    void submit_timeslice(uint64_t tpos) {
        out.info() << "timeslice submitted, position=" << tpos;
        for (auto& c : _conn) {
            out.info() << "timeslice number = " << c->_desc.at(tpos).ts_num;
        }
    }

};


#endif /* COMPUTEBUFFER_HPP */
