#pragma once
/**
 * \file Application.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#include "ThreadContainer.hpp"
#include "Parameters.hpp"
#include <vector>
#include <memory>

/// %Application base class.
/** The Application object represents an instance of the running
    application. */

template <typename T>
class Application : public ThreadContainer
{
public:
    /// The Application contructor.
    explicit Application(Parameters const& par) : _par(par)
    {
    }

    Application(const Application&) = delete;
    void operator=(const Application&) = delete;

    void start()
    {
        for (auto& buffer : _buffers) {
            buffer->start();
        }
    }

    void join()
    {
        for (auto& buffer : _buffers) {
            buffer->join();
        }
    }

protected:
    /// The run parameters object.
    Parameters const& _par;

    /// The application's connection group / buffer objects
    std::vector<std::unique_ptr<T> > _buffers;
};
