/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */
#pragma once

#include <string>
#include <stdexcept>

namespace flib {
class FlibException : public std::runtime_error {
public:
  explicit FlibException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};
}
