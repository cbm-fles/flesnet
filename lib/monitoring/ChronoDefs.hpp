// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_ChronoDefs
#define included_Cbm_ChronoDefs 1

#include <chrono>

namespace cbm {
using namespace std;

using sctime_point = chrono::system_clock::time_point;
using scduration = chrono::system_clock::duration;

} // end namespace cbm

#endif
