// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_PThreadHelper
#define included_Cbm_PThreadHelper 1

#include <string>

namespace cbm {
using namespace std;

void SetPThreadName(const string& name);
string PThreadName();

} // end namespace cbm

//#include "PThreadHelper.ipp"

#endif
