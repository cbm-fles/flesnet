/**
 * \file raw-storage.cpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "fles_ipc.hpp"
#include <iostream>
#include <csignal>

int count = 0;
FILE *fp;

/// The SIGTERM handler.
static void term_handler(int sig) {
    fclose(fp);
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

void store_timeslice(const fles::Timeslice& ts) {
    for (size_t c = 0; c < ts.num_components(); c++) {
        for (size_t m = 0; m < ts.total_size(); m++) {
            fwrite(&ts.descriptor(c, m), sizeof(fles::MicrosliceDescriptor), 1, fp);
            fwrite(ts.content(c, m), ts.descriptor(c, m).size, 1, fp);
        }
    }
}

int main() {
    fp = fopen("storage.data", "w");
    install_term_handler();
    
    fles::TimesliceReceiver tsr;
    
    while (true) {
        std::unique_ptr<const fles::Timeslice> ts = tsr.receive();
        store_timeslice(*ts);
        count++;
    }

    return 0;
}
