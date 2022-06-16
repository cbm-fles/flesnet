// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "ChronoHelper.hpp"

#include "fmt/format.h"

#include <stdio.h>
#include <time.h>

namespace cbm {
using namespace std;

//------------------------------------------------------------------------------
/*!
  \defgroup ChronoHelper Helper functions for chrono handling
*/

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Convert a chrono time_point to a ISO 8601 format string
  \param tpoint  chrono time_point
  \returns string representation of `tpoint`

  Converts `tpoint` into an [ISO 8601](https://en.wikipedia.org/wiki/ISO_8601)
  format string with micro second precision like `YYYY-MM-DDTHH:MM:SS.ssssss`.
*/

string TimePoint2String(const sctime_point& tpoint) {
  // get the date/time part up to the seconds level
  time_t tp_tt = chrono::system_clock::to_time_t(tpoint);
  tm tp_tm = {};
  (void)::localtime_r(&tp_tt, &tp_tm);
  // get fractional seconds part at microseconds precision
  auto tp_dur = tpoint.time_since_epoch();
  auto tp_sec = chrono::duration_cast<chrono::seconds>(tp_dur);
  auto tp_usec = chrono::duration_cast<chrono::microseconds>(tp_dur - tp_sec);
  return fmt::format("{:4d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:06d}",
                     tp_tm.tm_year + 1900, tp_tm.tm_mon + 1, tp_tm.tm_mday,
                     tp_tm.tm_hour, tp_tm.tm_min, tp_tm.tm_sec,
                     int(tp_usec.count()));
}

} // end namespace cbm
