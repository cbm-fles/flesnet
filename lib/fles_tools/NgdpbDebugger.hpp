// Copyright 2016 P.-A. Loizeau <p.-a.loizeau@gsi.de>
#pragma once

#include "TimesliceDebugger.hpp"
#include "Sink.hpp"
#include "Timeslice.hpp"
#include <ostream>

#pragma clang diagnostic ignored "-Wunused-private-field"

class NgdpbDump
{
public:
    NgdpbDump(const void* arg_buf, std::size_t arg_size)
        : buf(arg_buf), size(arg_size)
    {
    }
    friend std::ostream& operator<<(std::ostream& s, const NgdpbDump& dump);
    std::ostream& write_to_stream(std::ostream& s) const;

private:
    const void* buf;
    std::size_t size;
};

inline std::ostream& operator<<(std::ostream& s, const NgdpbDump& dump)
{
    return dump.write_to_stream(s);
}

// ----------

class NgdpbMicrosliceDumper : public fles::MicrosliceSink
{
public:
    NgdpbMicrosliceDumper(std::ostream& arg_out, std::size_t arg_verbosity)
        : out(arg_out), verbosity(arg_verbosity){};

    void put(std::shared_ptr<const fles::Microslice> m) override
    {
        // Update run statistics
        if (verbosity > 1) {
            // Dump content
            if (verbosity > 2){
               out << "-----------------------------"
                   << "\n";
               // Dump uS header statistics
               out << MicrosliceDescriptorDump(m->desc()) ;
            }
            out << NgdpbDump(m->content(), m->desc().size) ;
            if (verbosity > 2){
                out << "-----------------------------"
                    << "\n" ;
            }
        }
        // Dump uS statistics in destructor
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

