// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_FormatHelper
#define included_Cbm_FormatHelper 1

#include <string>

namespace cbm {
using namespace std;

string FmtD(long val, int width = 1);
string FmtD(int val, int width = 1);
string FmtD(short val, int width = 1);
string FmtD(char val, int width = 1);
string FmtD(unsigned long val, int width = 1);
string FmtD(unsigned int val, int width = 1);
string FmtD(unsigned short val, int width = 1);
string FmtD(unsigned char val, int width = 1);
string FmtX(unsigned long val, int width = 1, int prec = 1);
string FmtB(unsigned long val, int width = 1);
string FmtF(double val, int width = 5, int prec = 2);
string FmtE(double val, int width = 12, int prec = 6);

} // end namespace cbm

#include "FormatHelper.ipp"

#endif
