// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "PThreadHelper.hpp"

#include <pthread.h>

namespace cbm {
using namespace std;

//------------------------------------------------------------------------------
/*!
  \defgroup PThreadHelper Helper functions for thread handling
*/

//-----------------------------------------------------------------------------
/*!
  \brief Set thread name
  \ingroup PThreadHelper
  \param tname  thread name

  The method sets the thread name to `tname`. This name has no operational
  relevance and is only used in tools accessing them via `/proc`, like `top`.
  By convention the thread names of a \glos{CBMmain} start with "Cbm:".
  This allows for example to identify the threads of a \glos{CBM} in a threads
  view display of `top` (`H` mode).

  Worker instances set the thread name by convention to "Cbm:" +
  \glos{workerid}.

  \note The Linux kernel limits the thread name length to 16 characters.
    `tname` will be truncated if longer.
*/

void SetPThreadName(const string& tname) {
  // Note: the kernel limit is 16 char, see TASK_COMM_LEN; so truncate
  string tname16 = tname;
  if (tname16.length() > 16)
    tname16.resize(16);
  pthread_setname_np(pthread_self(), tname16.c_str());
}

//-----------------------------------------------------------------------------
/*!
  \brief Returns the thread name set via SetPThreadName()
  \ingroup PThreadHelper
*/

string PThreadName() {
  // Note: the kernel limit is 16 char, see TASK_COMM_LEN
  char tname[17] = {0};
  (void)::pthread_getname_np(pthread_self(), &tname[0], sizeof(tname) - 1);
  return string(tname);
}

} // end namespace cbm
