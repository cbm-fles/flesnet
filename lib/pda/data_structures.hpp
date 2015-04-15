/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */
#pragma once

#include <stdexcept>

namespace pda {
class PdaException : public std::runtime_error {
public:
  explicit PdaException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};
}

#ifndef PAGE_MASK
#define PAGE_MASK ~(sysconf(_SC_PAGESIZE) - 1)
#endif
 
#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif
