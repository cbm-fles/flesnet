// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

#include "TimesliceAnalyzer.hpp"
#include "TimesliceReceiver.hpp"
#include "TimesliceInputArchive.hpp"
#include <iostream>
#include <memory>
#include <cassert>

int main(int argc, char* argv[])
{
    assert(argc > 1);
    std::string param(argv[1]);

    std::unique_ptr<fles::TimesliceSource> ts_source;

    if (param.find("flesnet_") == 0) {
        ts_source.reset(new fles::TimesliceReceiver(param));
    } else {
        ts_source.reset(new fles::TimesliceInputArchive(param));
    }

    TimesliceAnalyzer ana;

    while (auto ts = ts_source->get()) {
        ana.check_timeslice(*ts);
        if ((ana.count() % 10000) == 0) {
            std::cout << ana.statistics() << std::endl;
            ana.reset();
        }
    }

    std::cout << "total timeslices checked: " << ana.count() << std::endl;

    return 0;
}
