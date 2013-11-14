// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceReceiver.hpp"
#include "TimesliceOutputArchive.hpp"

int main(int argc, char* argv[])
{
    assert(argc > 1);

    fles::TimesliceReceiver tsr(argv[1]);
    fles::TimesliceOutputArchive output_archive("storage.data");

    int count = 0;
    while (auto timeslice = tsr.receive()) {
        output_archive.write(*timeslice);
        ++count;
    }
    std::cout << "timeslices stored: " << count << std::endl;

    return 0;
}
