// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Parameters.hpp"
#include "TimesliceSource.hpp"
#include "TimesliceOutputArchive.hpp"
#include "TimesliceAnalyzer.hpp"
#include "TimesliceDebugger.hpp"
#include "TimeslicePublisher.hpp"
#include "Benchmark.hpp"
#include <memory>

/// %Application base class.
class Application
{
public:
    explicit Application(Parameters const& par);

    Application(const Application&) = delete;
    void operator=(const Application&) = delete;

    ~Application();

    void run();

private:
    Parameters const& par_;

    std::unique_ptr<fles::TimesliceSource> source_;
    std::unique_ptr<TimesliceAnalyzer> analyzer_;
    std::unique_ptr<fles::TimesliceOutputArchive> output_;
    std::unique_ptr<fles::TimeslicePublisher> publisher_;
    std::unique_ptr<Benchmark> benchmark_;

    uint64_t count_ = 0;
};
