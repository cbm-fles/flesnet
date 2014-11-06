// Copyright 2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Timeslice.hpp"
#include <memory>

namespace fles
{

//! The TimesliceSource base class implements the generic timeslice-based input
// interface.
class TimesliceSource
{
public:
    /// Retrieve the next timeslice, block if not yet available. Return nullptr
    /// if eof.
    std::unique_ptr<Timeslice> get()
    {
        return std::unique_ptr<Timeslice>(do_get());
    };

    virtual ~TimesliceSource() {};

protected:
    bool _eof = false;

private:
    virtual Timeslice* do_get() = 0;
};

} // namespace fles {
