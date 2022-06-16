// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "SysCallException.hpp"

#include "fmt/format.h"

#include <string.h>

namespace cbm {
using namespace std;

/*! \class SysCallException
  \brief Exception with context and errno information
 */

//-----------------------------------------------------------------------------
/*! \brief Constructor with 2 part context and errno information
  \param where  string describing class and method
  \param call   string describing the system call which failed
  \param eno    errno value
  Typical usage
\code{.cpp}
  int fd = ::eventfd(0U, 0);
  if (fd < 0)
    throw SysCallException("Logger::ctor"s, "eventfd"s, errno);
\endcode
*/

SysCallException::SysCallException(const string& where,
                                   const string& call,
                                   int eno)
    : Exception(
          fmt::format("{}: {} failed: {}", where, call, ::strerror(eno))) {}

//-----------------------------------------------------------------------------
/*! \brief Constructor with 3 part context and errno information
  \param where  string describing class and method
  \param call   string describing the system call which failed
  \param info   string with additional context information
  \param eno    errno value

  Typical usage
\code{.cpp}
  if (::write(fEvtFd, &one, sizeof(one)) != sizeof(one))
    throw SysCallException("Worker::Wakeup"s, "write"s, "fEvtFd"s, errno);
\endcode
*/

SysCallException::SysCallException(const string& where,
                                   const string& call,
                                   const string& info,
                                   int eno)
    : Exception(fmt::format(
          "{}: {} for '{}' failed: {}", where, call, info, ::strerror(eno))) {}

} // end namespace cbm
