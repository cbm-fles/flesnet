// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Parameters.hpp"
#include "TimesliceSource.hpp"
#include "TimesliceOutputArchive.hpp"
#include "TimesliceAnalyzer.hpp"
#include "TimesliceDebugger.hpp"
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
    Parameters const& _par;

    std::unique_ptr<fles::TimesliceSource> _source;
    std::unique_ptr<TimesliceAnalyzer> _analyzer;
    std::unique_ptr<TimesliceDebugger> _dump;
    std::unique_ptr<fles::TimesliceOutputArchive> _output;

    uint64_t _count = 0;
};
