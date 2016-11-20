// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Benchmark.hpp"
#include "Parameters.hpp"
#include "Sink.hpp"
#include "TimesliceSource.hpp"
#include <memory>
#include <vector>

#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/stream.hpp>
#include <iosfwd>
#include <iostream>
#include <log.hpp>

class LogBuffer
{
public:
    typedef char char_type;
    typedef boost::iostreams::sink_tag category;

    std::streamsize write(char_type const* s, std::streamsize n)
    {
        L_(status) << std::string(s, n);
        return n;
    }
};

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
    std::vector<std::unique_ptr<fles::TimesliceSink>> sinks_;
    std::unique_ptr<Benchmark> benchmark_;

    uint64_t count_ = 0;

    boost::iostreams::stream_buffer<LogBuffer> log_buf_;
    std::ostream logstream_;
};
