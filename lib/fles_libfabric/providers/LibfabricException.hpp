// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include <stdexcept>

namespace tl_libfabric {
/// Libfabric exception class.
/** An LibfabricException object signals an error that occured in the
    Libfabric communication functions. */

class LibfabricException : public std::runtime_error {
public:
  /// The LibfabricException default constructor.
  explicit LibfabricException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};
} // namespace tl_libfabric
