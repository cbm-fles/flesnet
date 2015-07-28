// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Parameters.hpp"
#include "DataSource.hpp"
#include "MicrosliceOutputArchive.hpp"
#include "MicrosliceSource.hpp"
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

    std::unique_ptr<DataSource> data_source_;
    std::unique_ptr<fles::MicrosliceSource> source_;
    std::unique_ptr<fles::MicrosliceOutputArchive> output_;

    uint64_t count_ = 0;
};
