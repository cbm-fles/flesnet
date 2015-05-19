// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <memory>

namespace fles
{

//! The Source base class implements the generic item-based input
// interface.
template <class T> class Source
{
public:
    /// Retrieve the next item, block if not yet available. Return nullptr
    /// if eof.
    std::unique_ptr<T> get() { return std::unique_ptr<T>(do_get()); };

    virtual ~Source(){};

protected:
    bool _eof = false;

private:
    virtual T* do_get() = 0;
};

} // namespace fles {
