// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Microslice.hpp"
#include <memory>

namespace fles
{

//! The MicrosliceSource base class implements the generic microslice-based
// input
// interface.
class MicrosliceSource
{
public:
    /// Retrieve the next microslice, block if not yet available. Return nullptr
    /// if eof.
    std::unique_ptr<Microslice> get()
    {
        return std::unique_ptr<Microslice>(do_get());
    };

    virtual ~MicrosliceSource(){};

protected:
    bool _eof = false;

private:
    virtual Microslice* do_get() = 0;
};

} // namespace fles {
