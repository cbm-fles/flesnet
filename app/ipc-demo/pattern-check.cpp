/**
 * \file pattern-check.cpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "fles_ipc.hpp"
#include <iostream>
#include <csignal>

int count = 0;

/// The SIGTERM handler.
static void term_handler(int sig)
{
    std::cout << "timeslices checked: " << count << std::endl;
    exit(0);
}

/// Establish SIGTERM handler.
void install_term_handler()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = term_handler;
    sigaction(SIGTERM, &sa, nullptr);
}

bool check_flesnet_pattern(const fles::MicrosliceDescriptor& descriptor,
                           const uint64_t* content, size_t component)
{
    uint32_t crc = 0x00000000;
    for (size_t pos = 0; pos < descriptor.size; pos += sizeof(uint64_t)) {
        uint64_t data_word = content[pos / sizeof(uint64_t)];
        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32);
        uint64_t expected = (static_cast<uint64_t>(component) << 48) | pos;
        if (data_word != expected)
            return false;
    }
    if (crc != descriptor.crc)
        return false;
    return true;
}

bool check_flib_pattern(const fles::MicrosliceDescriptor& descriptor,
                        const uint64_t* content, size_t component)
{
    if (content[0] != reinterpret_cast<const uint64_t*>(&descriptor)[0]
        || content[1] != reinterpret_cast<const uint64_t*>(&descriptor)[1]) {
        return false;
    }
    return true;
}

bool check_microslice(const fles::MicrosliceDescriptor& descriptor,
                      const uint64_t* content, size_t component,
                      size_t microslice)
{
    if (descriptor.idx != microslice) {
        std::cerr << "microslice index " << descriptor.idx
                  << " found in descriptor " << microslice << std::endl;
        return false;
    }

    switch (descriptor.sys_id) {
    case 0x01:
        return check_flesnet_pattern(descriptor, content, component);
    case 0xBC:
        return check_flib_pattern(descriptor, content, component);
    default:
        std::cerr << "unknown subsystem identifier" << std::endl;
        return false;
    }
    return true;
}

bool check_timeslice(const fles::Timeslice& ts)
{
    for (size_t c = 0; c < ts.num_components(); ++c) {
        for (size_t m = 0; m < ts.num_microslices(); ++m) {
            bool success = check_microslice(
                ts.descriptor(c, m),
                reinterpret_cast<const uint64_t*>(ts.content(c, m)), c,
                ts.index() * ts.num_core_microslices() + m);
            if (!success) {
                std::cerr << "-- pattern error in TS " << ts.index() << ", MC "
                          << m << ", component " << c << std::endl;
                return false;
            }
        }
    }
    return true;
}

int main(int argc, char* argv[])
{
    install_term_handler();

    fles::TimesliceReceiver tsr(argv[1]);

    while (true) {
        std::unique_ptr<const fles::Timeslice> ts = tsr.receive();
        check_timeslice(*ts);
        ++count;
    }

    return 0;
}
