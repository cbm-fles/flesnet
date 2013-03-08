/**
 * \file ComputeBuffer.hpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef COMPUTEBUFFER_HPP
#define COMPUTEBUFFER_HPP

#include "ComputeNodeConnection.hpp"


/// Compute buffer and input node connection container class.
/** A ComputeBuffer object represents a timeslice buffer (filled by
    the input nodes) and a group of timeslice building connections to
    input nodes. */

class ComputeBuffer : public IBConnectionGroup<ComputeNodeConnection>
{
public:
    
    /// Completion notification event dispatcher. Called by the event loop.
    virtual void on_completion(const struct ibv_wc& wc) {
        switch (wc.wr_id & 0xFF) {
        case ID_SEND_CN_ACK:
            out.debug() << "SEND complete";
            break;

        case ID_SEND_FINALIZE:
            out.debug() << "SEND FINALIZE complete";
            _all_done = true;
            break;

        case ID_RECEIVE_CN_WP:
            _conn[0]->on_complete_recv();
            break;

        default:
            throw InfinibandException("wc for unknown wr_id");
        }
    }
};


#endif /* COMPUTEBUFFER_HPP */
