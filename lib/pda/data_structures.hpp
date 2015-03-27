/**
 * @file
 * @author Dirk Hutter <hutter@compeng.uni-frankfurt.de>
 * @author Dominic Eschweiler<dominic.eschweiler@cern.ch>
 *
 */
#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

#include <cstdint>
#include <unistd.h>

#include <string>
#include <stdexcept>

namespace pda {
class PdaException : public std::runtime_error {
public:
  explicit PdaException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};
}

#define RORCFS_DMA_FROM_DEVICE 2
#define RORCFS_DMA_TO_DEVICE 1
#define RORCFS_DMA_BIDIRECTIONAL 0
 
#ifndef PAGE_MASK
#define PAGE_MASK ~(sysconf(_SC_PAGESIZE) - 1)
#endif
 
#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif

#pragma GCC diagnostic pop
