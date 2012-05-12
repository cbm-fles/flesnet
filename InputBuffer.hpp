/*
 * \file InputBuffer.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef INPUTBUFFER_HPP
#define INPUTBUFFER_HPP

#include "ComputeNodeConnection.hpp"
#include "log.hpp"


/// Input buffer and compute node connection container class.
/** An InputBuffer object represents an input buffer (filled by a
    FLIB) and a group of timeslice building connections to compute
    nodes. */

class InputBuffer : public IBConnectionGroup<ComputeNodeConnection>
{
public:

    /// The InputBuffer default constructor.
    InputBuffer() :
        _acked_mc(0), _acked_data(0),
        _mc_written(0), _data_written(0) {
        _data = new uint64_t[DATA_WORDS]();
        _addr = new uint64_t[ADDR_WORDS]();
        _ack = new uint64_t[ACK_WORDS]();
    };

    /// The InputBuffer default destructor.
    ~InputBuffer() {
        delete [] _data;
        delete [] _addr;
        delete [] _ack;
    };

    /// The central loop for distributing timeslice data.
    void senderLoop() {
        for (uint64_t timeslice = 0; timeslice < NUM_TS; timeslice++) {

            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            
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

    /// Register memory regions.
    void setup() {
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

    /// Input data buffer. Filled by FLIB.
    uint64_t* _data;

    /// InfiniBand memory region descriptor for input data buffer.
    struct ibv_mr* _mr_data;

    /// Input address buffer. Filled by FLIB.
    uint64_t* _addr;

    /// InfiniBand memory region descriptor for input address buffer.
    struct ibv_mr* _mr_addr;

    /// Buffer to store acknowledged status of timeslices.
    uint64_t* _ack;

    /// Number of acknowledged MCs. Can be read by FLIB.
    uint64_t _acked_mc;
    
    /// Number of acknowledged data words. Can be read by FLIB.
    uint64_t _acked_data;
    
    /// FLIB-internal number of written MCs. 
    uint64_t _mc_written;

    /// FLIB-internal number of written data words. 
    uint64_t _data_written;

    /// Return target computation node for given timeslice.
    int target_cn_index(uint64_t timeslice) {
        return timeslice % _conn.size();
    }

    /// Generate FLIB input data.
    void wait_for_data(uint64_t min_mc_number) {
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
                boost::this_thread::sleep(boost::posix_time::millisec(1000));
                break;
            }

            // check for space in addr buffer
            if (_mc_written - _acked_mc == ADDR_WORDS) {
                Log.warn() << "addr buffer full";
                boost::this_thread::sleep(boost::posix_time::millisec(1000));
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

    /// Return string describing buffer contents, suitable for debug output.
    std::string getStateString() {
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

    /// Create gather list for transmission of timeslice
    void post_send_data(uint64_t timeslice, int cn,
                        uint64_t mc_offset, uint64_t mc_length,
                        uint64_t data_offset, uint64_t data_length) {
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

        _conn[cn]->sendData(sge, num_sge, timeslice, mc_length, data_length);
    }

    /// Completion notification event dispatcher. Called by the event loop.
    virtual void onCompletion(const struct ibv_wc& wc) {
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
};


#endif /* INPUTBUFFER_HPP */
