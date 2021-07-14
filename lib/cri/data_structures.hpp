/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 *
 */
#pragma once

#include <stdexcept>
#include <string>

namespace cri {
class CriException : public std::runtime_error {
public:
  explicit CriException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};
} // namespace cri
