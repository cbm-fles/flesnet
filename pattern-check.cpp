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
static void term_handler(int sig) {
    std::cout << "timeslices checked: " << count << std::endl;
    exit(0);
}

/// Establish SIGTERM handler.
void install_term_handler() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = term_handler;
    sigaction(SIGTERM, &sa, NULL);
}

void check_timeslice(const fles::Timeslice& ts) {
    for (size_t c = 0; c < ts.num_components(); c++) {
        for (size_t m = 0; m < ts.total_size(); m++) {
            const uint64_t* content = reinterpret_cast<const uint64_t*>(ts.content(c, m));
            uint32_t crc = 0x00000000;
            for (size_t pos = 0; pos < ts.descriptor(c, m).size; pos += sizeof(uint64_t)) {
                uint64_t data_word = content[pos / sizeof(uint64_t)];
                crc ^= (data_word & 0xffffffff) ^ (data_word >> 32);
                uint64_t expected = ((uint64_t) c << 48) | pos;
                assert(data_word == expected);
            }
            assert(crc == ts.descriptor(c, m).crc);
        }
    }
}

int main() {
    install_term_handler();
    
    fles::TimesliceReceiver tsr;
    
    while (true) {
        std::unique_ptr<const fles::Timeslice> ts = tsr.receive();
        check_timeslice(*ts);
        count++;
    }

    return 0;
}
