// Copyright 2012-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <stdexcept>

/// InfiniBand exception class.
/** An InfinbandException object signals an error that occured in the
    InfiniBand communication functions. */

class InfinibandExceptionUcx : public std::runtime_error {
public:
  /// The InfinibandExceptionUcx default constructor.
  explicit InfinibandExceptionUcx(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};
