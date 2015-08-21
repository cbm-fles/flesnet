// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Benchmark.hpp"
#include "Parameters.hpp"
#include "Sink.hpp"
#include "TimesliceAnalyzer.hpp"
#include "TimesliceSource.hpp"
#include <memory>
#include <vector>

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
    std::vector<std::unique_ptr<fles::TimesliceSink>> sinks_;
    std::unique_ptr<Benchmark> benchmark_;

    uint64_t count_ = 0;
};
