/**
 * \file InputBuffer.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef INPUTBUFFER_HPP
#define INPUTBUFFER_HPP

#include <cassert>
#include "ComputeNodeConnection.hpp"
#include "DataSource.hpp"
#include "global.hpp"


/// Input buffer and compute node connection container class.
/** An InputBuffer object represents an input buffer (filled by a
    FLIB) and a group of timeslice building connections to compute
    nodes. */

class InputBuffer : public IBConnectionGroup<ComputeNodeConnection>
{
public:

    /// The InputBuffer default constructor.
    InputBuffer() :
        _data(Par->inDataBufferSizeExp()), _mr_data(0),
        _addr(Par->inAddrBufferSizeExp()), _mr_addr(0),
        _acked_mc(0), _acked_data(0),
        _senderLoopDone(false), _connectionsDone(0),
        _aggregateContentBytesSent(0),
        _aggregateBytesSent(0),
        _aggregateSendRequests(0),
        _aggregateRecvRequests(0),
        _dataSource(_data, _addr)
    {
        size_t minAckBufferSize = _addr.size() / Par->timesliceSize() + 1;
        _ack.allocWithSize(minAckBufferSize);
    }

    /// The InputBuffer default destructor.
    ~InputBuffer() { }

    /// The central loop for distributing timeslice data.
    void senderLoop() {
        for (uint64_t timeslice = 0; timeslice < Par->maxTimesliceNumber();
             timeslice++) {

            // wait until a complete TS is available in the input buffer
            uint64_t mc_offset = timeslice * Par->timesliceSize();
            uint64_t mc_length = Par->timesliceSize() + Par->overlapSize();
            
            if (_addr.at(mc_offset + mc_length) <= _acked_data)
                _dataSource.waitForData(mc_offset + mc_length + 1);
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
                Log.trace() << getStateString();
            }

            int cn = target_cn_index(timeslice);

            _conn[cn]->waitForBufferSpace(data_length + mc_length, 1);

            post_send_data(timeslice, cn, mc_offset, mc_length,
                           data_offset, data_length);

            _conn[cn]->incWritePointers(data_length + mc_length, 1);

        }

        Log.info() << "SENDER loop done";
    }

    /// Handle RDMA_CM_EVENT_DISCONNECTED event.
    virtual int onDisconnect(struct rdma_cm_id* id) {
        ComputeNodeConnection* conn = (ComputeNodeConnection*) id->context;

        _aggregateContentBytesSent += conn->contentBytesSent();
        _aggregateBytesSent += conn->totalBytesSent();
        _aggregateSendRequests += conn->totalSendRequests();
        _aggregateRecvRequests += conn->totalRecvRequests();        

        return IBConnectionGroup<ComputeNodeConnection>::onDisconnect(id);
    }

    /// Retrieve the number of bytes transmitted (without pointer updates).
    uint64_t aggregateContentBytesSent() const {
        return _aggregateContentBytesSent;
    }

    /// Retrieve the total number of bytes transmitted.
    uint64_t aggregateBytesSent() const {
        return _aggregateBytesSent;
    }

    /// Retrieve the total number of SEND work requests.
    uint64_t aggregateSendRequests() const {
        return _aggregateSendRequests;
    }

    /// Retrieve the total number of RECV work requests.
    uint64_t aggregateRecvRequests() const {
        return _aggregateRecvRequests;
    }


private:

    /// Input data buffer. Filled by FLIB.
    RingBuffer<uint64_t> _data;

    /// InfiniBand memory region descriptor for input data buffer.
    struct ibv_mr* _mr_data;

    /// Input address buffer. Filled by FLIB.
    RingBuffer<uint64_t> _addr;

    /// InfiniBand memory region descriptor for input address buffer.
    struct ibv_mr* _mr_addr;

    /// Buffer to store acknowledged status of timeslices.
    RingBuffer<uint64_t> _ack;

    /// Number of acknowledged MCs. Written to FLIB.
    uint64_t _acked_mc;
    
    /// Number of acknowledged data words. Written to FLIB.
    uint64_t _acked_data;
    
    /// Flag indicating completion of the sender loop for this run.
    bool _senderLoopDone;

    /// Number of connections in the done state.
    unsigned int _connectionsDone;

    /// Total number of bytes transmitted (without pointer updates).
    uint64_t _aggregateContentBytesSent;

    /// Total number of bytes transmitted.
    uint64_t _aggregateBytesSent;

    /// Total number of SEND work requests.
    uint64_t _aggregateSendRequests;

    /// Total number of RECV work requests.
    uint64_t _aggregateRecvRequests;

    /// Data source (e.g., FLIB).
    DummyFlib _dataSource;
    
    /// Return target computation node for given timeslice.
    int target_cn_index(uint64_t timeslice) {
        return timeslice % _conn.size();
    }

    /// Handle RDMA_CM_EVENT_ADDR_RESOLVED event.
    virtual int onAddrResolved(struct rdma_cm_id* id) {
        int ret = IBConnectionGroup<ComputeNodeConnection>::onAddrResolved(id);

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
        
        return ret;
    }
    
    /// Return string describing buffer contents, suitable for debug output.
    std::string getStateString() {
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
        if ((mc_offset & _addr.sizeMask())
            < ((mc_offset + mc_length - 1) & _addr.sizeMask())) {
            // one chunk
            sge[num_sge].addr = (uintptr_t) &_addr.at(mc_offset);
            sge[num_sge].length = sizeof(uint64_t) * mc_length;
            sge[num_sge++].lkey = _mr_addr->lkey;
        } else {
            // two chunks
            sge[num_sge].addr = (uintptr_t) &_addr.at(mc_offset);
            sge[num_sge].length =
                sizeof(uint64_t) * (_addr.size()
                                    - (mc_offset & _addr.sizeMask()));
            sge[num_sge++].lkey = _mr_addr->lkey;
            sge[num_sge].addr = (uintptr_t) _addr.ptr();
            sge[num_sge].length =
                sizeof(uint64_t) * (mc_length - _addr.size()
                                    + (mc_offset & _addr.sizeMask()));
            sge[num_sge++].lkey = _mr_addr->lkey;
        }
        // data words
        if ((data_offset & _data.sizeMask())
            < ((data_offset + data_length - 1) & _data.sizeMask())) {
            // one chunk
            sge[num_sge].addr = (uintptr_t) &_data.at(data_offset);
            sge[num_sge].length = sizeof(uint64_t) * data_length;
            sge[num_sge++].lkey = _mr_data->lkey;
        } else {
            // two chunks
            sge[num_sge].addr = (uintptr_t) &_data.at(data_offset);
            sge[num_sge].length =
                sizeof(uint64_t)
                * (_data.size() - (data_offset & _data.sizeMask()));
            sge[num_sge++].lkey = _mr_data->lkey;
            sge[num_sge].addr = (uintptr_t) _data.ptr();
            sge[num_sge].length =
                sizeof(uint64_t) * (data_length - _data.size()
                                    + (data_offset & _data.sizeMask()));
            sge[num_sge++].lkey = _mr_data->lkey;
        }

        _conn[cn]->sendData(sge, num_sge, timeslice, mc_length, data_length);
    }

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void onCompletion(const struct ibv_wc& wc) {
        switch (wc.wr_id & 0xFF) {
        case ID_WRITE_DESC: {
            uint64_t ts = wc.wr_id >> 8;
            Log.debug() << "write completion for timeslice " << ts;

            uint64_t acked_ts = _acked_mc / Par->timesliceSize();
            if (ts == acked_ts)
                do
                    acked_ts++;
                while (_ack.at(acked_ts) > ts);
            else
                _ack.at(ts) = ts;
            _acked_data = _addr.at(acked_ts * Par->timesliceSize());
            _acked_mc = acked_ts * Par->timesliceSize();
            _dataSource.updateAckPointers(_acked_data, _acked_mc);
            Log.debug() << "new values: _acked_data="
                        << _acked_data
                        << " _acked_mc=" << _acked_mc;
            if (acked_ts == Par->maxTimesliceNumber() - 1) {
                _senderLoopDone = true;                
                for(auto it = _conn.begin(); it != _conn.end(); ++it)
                    _connectionsDone += (*it)->doneIfCnEmpty();
                _allDone = (_connectionsDone == _conn.size());
            }
        }
        break;

        case ID_RECEIVE_CN_ACK: {
            int cn = wc.wr_id >> 8;
            _conn[cn]->onCompleteRecv();
            if (_senderLoopDone) {
                _connectionsDone += _conn[cn]->doneIfCnEmpty();
                _allDone = (_connectionsDone == _conn.size());
            }
        }
        break;

        default:
            throw InfinibandException("wc for unknown wr_id");
        }
    }
};


#endif /* INPUTBUFFER_HPP */
