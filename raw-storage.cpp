/**
 * \file raw-storage.cpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "fles_ipc.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <fstream>
#include <iostream>
#include <csignal>

int count = 0;

/// The SIGTERM handler.
static void term_handler(int sig) {
    std::cout << "timeslices stored: " << count << std::endl;
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

int main() {
    install_term_handler();

    std::ofstream ofs("storage.data");
    boost::archive::binary_oarchive oa(ofs);

    fles::TimesliceReceiver tsr;
    
    while (true) {
        std::unique_ptr<const fles::Timeslice> ts = tsr.receive();
        fles::StorableTimeslice sts(*ts);
        oa << sts;
        ++count;
    }

    return 0;
}
