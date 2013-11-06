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
static void term_handler(int /* sig */)
{
    std::cout << "timeslices stored: " << count << std::endl;
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

int main(int argc, char* argv[])
{
    assert(argc > 1);

    install_term_handler();

    std::ofstream ofs("storage.data");
    boost::archive::binary_oarchive output_archive(ofs);

    fles::TimesliceReceiver tsr(argv[1]);

    while (true) {
        std::unique_ptr<const fles::TimesliceView> tsv = tsr.receive();
        fles::StorableTimeslice sts(*tsv);
        output_archive << sts;
        ++count;
    }

    return 0;
}
