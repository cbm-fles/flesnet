// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020-2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_ChronoHelper
#define included_Cbm_ChronoHelper 1

#include "ChronoDefs.hpp"

#include <string>

namespace cbm {
using namespace std;

sctime_point ScNow();

string TimePoint2String(const sctime_point& time);
string TimeStamp();

long ScDuration2Msec(const scduration& dur);
long ScDuration2Usec(const scduration& dur);
long ScDuration2Nsec(const scduration& dur);
scduration Msec2ScDuration(long msec);
scduration Usec2ScDuration(long usec);
scduration Nsec2ScDuration(long nsec);
long ScTimePoint2Nsec(const sctime_point& time);
long ScTimePoint2Nsec();
sctime_point Nsec2ScTimePoint(long nsec);
double ScTimeDiff2Double(const sctime_point& tbeg, const sctime_point& tend);

} // end namespace cbm

#include "ChronoHelper.ipp"

#endif
