/**
 * \file DataSource.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef DATASOURCE_HPP
#define DATASOURCE_HPP

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/poisson_distribution.hpp>
#include <boost/random/variate_generator.hpp>
#include "RingBuffer.hpp"
#include "global.hpp"


/// Abstract FLES data source class.
class DataSource
{
public:
    /// The DataSource constructor.
    DataSource(RingBuffer<uint64_t>& dataBuffer,
               RingBuffer<uint64_t>& addrBuffer) :
        _dataBuffer(dataBuffer),
        _addrBuffer(addrBuffer) { };
    
    virtual void waitForData(uint64_t minMcNumber) = 0;

    virtual void updateAckPointers(uint64_t ackedData, uint64_t ackedMc) = 0;
    
protected:
    /// Input data buffer.
    RingBuffer<uint64_t>& _dataBuffer;

    /// Input address buffer.
    RingBuffer<uint64_t>& _addrBuffer;
};


/// Simple software pattern generator used as FLIB replacement.
class DummyFlib : DataSource
{
public:
    /// The DummyFlib constructor.
    DummyFlib(RingBuffer<uint64_t>& dataBuffer,
              RingBuffer<uint64_t>& addrBuffer) :
        DataSource(dataBuffer, addrBuffer),
        _pd(Par->typicalContentSize()),
        _randContentWords(_rng, _pd) { };
    
    /// Generate FLIB input data.
    virtual void waitForData(uint64_t minMcNumber) {
        
        uint64_t mcsToWrite = minMcNumber - _mcWritten;

        if (Par->randomizeSizes()) {
            // write more data than requested (up to 2 additional TSs)
            mcsToWrite += random() % (Par->timesliceSize() * 2);
        }

        if (Log.beTrace()) {
            Log.trace() << "waitForData():"
                        << " minMcNumber=" << minMcNumber
                        << " _mcWritten=" << _mcWritten
                        << " mcsToWrite= " << mcsToWrite;
        }

        while (mcsToWrite-- > 0) {
            int contentWords = Par->typicalContentSize();
            if (Par->randomizeSizes())
                contentWords = _randContentWords();

            uint8_t hdrrev = 0x01;
            uint8_t sysid = 0x01;
            uint16_t flags = 0x0000;
            uint32_t size = (contentWords + 2) * 8;
            uint16_t rsvd = 0x0000;
            uint64_t time = _mcWritten;

            uint64_t hdr0 = (uint64_t) hdrrev << 56 | (uint64_t) sysid << 48
                            | (uint64_t) flags << 32 | (uint64_t) size;
            uint64_t hdr1 = (uint64_t) rsvd << 48 | (time & 0xFFFFFFFFFFFF);

            if (Log.beTrace()) {
                Log.trace() << "waitForData():"
                            << " _dataWritten=" << _dataWritten
                            << " _ackedData=" << _ackedData
                            << " contentWords=" << contentWords
                            << " Par->inDataBufferSize()="
                            << _dataBuffer.size() + 0;
            }
            
            // check for space in data buffer, busy wait if required
            if (_dataWritten - _ackedData + contentWords + 2 >
                _dataBuffer.size()) {
                if (Log.beTrace())
                    Log.trace() << "data buffer full";
                boost::this_thread::sleep(boost::posix_time::millisec(10));
                break;
            }

            // check for space in addr buffer, busy wait if required
            if (_mcWritten - _ackedMc == _addrBuffer.size()) {
                if (Log.beTrace())
                    Log.trace() << "addr buffer full";
                boost::this_thread::sleep(boost::posix_time::millisec(10));
                break;
            }

            // write to data buffer
            uint64_t startAddr = _dataWritten;
            _dataBuffer.at(_dataWritten++) = hdr0;
            _dataBuffer.at(_dataWritten++) = hdr1;

            for (int i = 0; i < contentWords; i++) {
                _dataBuffer.at(_dataWritten++) = i + 0xA;
            }

            // write to addr buffer
            _addrBuffer.at(_mcWritten++) = startAddr;
        }
    };

    virtual void updateAckPointers(uint64_t ackedData, uint64_t ackedMc) {
        _ackedData = ackedData;
        _ackedMc = ackedMc;
    };
            
private:
    /// Number of acknowledged data words. Updated by input node.
    uint64_t _ackedData = 0;
    
    /// Number of acknowledged MCs. Updated by input node.
    uint64_t _ackedMc = 0;
    
    /// FLIB-internal number of written data words. 
    uint64_t _dataWritten = 0;

    /// FLIB-internal number of written MCs. 
    uint64_t _mcWritten = 0;

    /// A pseudo-random number generator.
    boost::mt19937 _rng;

    /// Distribution to use in determining data content sizes.
    boost::poisson_distribution<> _pd;

    /// Generate random number of content words.
    boost::variate_generator<boost::mt19937&,
                             boost::poisson_distribution<> > _randContentWords;
};
    

#endif /* DATASOURCE_HPP */
