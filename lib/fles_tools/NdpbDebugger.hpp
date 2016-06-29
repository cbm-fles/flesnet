// Copyright 2016 P.-A. Loizeau <p.-a.loizeau@gsi.de>
#pragma once

#include "TimesliceDebugger.hpp"
#include "Sink.hpp"
#include "Timeslice.hpp"
#include <ostream>

class NdpbDump
{
public:
    NdpbDump(const void* arg_buf, std::size_t arg_size)
        : buf(arg_buf), size(arg_size)
    {
    }
    friend std::ostream& operator<<(std::ostream& s, const NdpbDump& dump);
    std::ostream& write_to_stream(std::ostream& s) const;

private:
    const void* buf;
    std::size_t size;
/*
    inline uint32_t getField( uint64_t data, uint32_t shift, uint32_t len) const
      { return (data >> shift) & (((static_cast<uint32_t>(1)) << len) - 1); }
    inline uint16_t getBit( uint64_t data, int32_t shift) const
      { return (data >> shift) & 1; }

    inline uint32_t getFieldBE( uint64_t data, uint32_t shift, uint32_t len) const
      { return (dataBE(data) >> shift) & (((static_cast<uint32_t>(1)) << len) - 1); }
    inline uint16_t getBitBE( uint64_t data, uint32_t shift) const
      { return (dataBE(data) >> shift) & 1; }

    inline uint64_t dataBE( uint64_t data ) const
      { return ((data&0x00000000000000FF)<<56)+
               ((data&0x000000000000FF00)<<40)+
               ((data&0x0000000000FF0000)<<24)+
               ((data&0x00000000FF000000)<< 8)+
               ((data>> 8)&0x00000000FF000000)+
               ((data>>24)&0x0000000000FF0000)+
               ((data>>40)&0x000000000000FF00)+
               ((data>>56)&0x00000000000000FF);
               }
*/
};

inline std::ostream& operator<<(std::ostream& s, const NdpbDump& dump)
{
    return dump.write_to_stream(s);
}

// ----------

class NdpbMicrosliceDumper : public fles::MicrosliceSink
{
public:
    NdpbMicrosliceDumper(std::ostream& arg_out, std::size_t arg_verbosity)
        : out(arg_out), verbosity(arg_verbosity){};

    void put(std::shared_ptr<const fles::Microslice> m) override
    {
        // Update run statistics
        if (verbosity > 1) {
            // Dump uS statistics
            out << MicrosliceDescriptorDump(m->desc()) ;
            if (verbosity > 2){
               // Dump content
               out << "-----------------------------"
                   << "\n";
               out << NdpbDump(m->content(), m->desc().size) 
                   << "-----------------------------"
                   << "\n";
            }
        }
    }

private:
    std::ostream& out;
    std::size_t verbosity;

    uint32_t fuNbNop     = 0;
    uint32_t fuNbHit     = 0;
    uint32_t fuNbEpoch   = 0;
    uint32_t fuNbSync    = 0;
    uint32_t fuNbAux     = 0;
    uint32_t fuNbEpochG  = 0;
    uint32_t fuNbGet4    = 0;
    uint32_t fuNbSys     = 0;
    uint32_t fuNbGet4_32 = 0;
    uint32_t fuNbGet4Sys = 0;
};

