// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <stdexcept>

/// Libfabric exception class.
/** An LibfabricException object signals an error that occured in the
    Libfabric communication functions. */

class LibfabricException : public std::runtime_error
{
public:
    /// The LibfabricException default constructor.
    explicit LibfabricException(const std::string &what_arg = "")
        : std::runtime_error(what_arg)
    {
    }
};
