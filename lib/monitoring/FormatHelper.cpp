// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "FormatHelper.hpp"

#include <limits>

#include <stdio.h>

namespace cbm {
using namespace std;

//------------------------------------------------------------------------------
/*!
  \defgroup FormatHelper Helper functions for formatting
*/

//-----------------------------------------------------------------------------
/*!
  \ingroup FormatHelper
  \brief Convert integer with %%d (decimal)
  \param val     value to be converted
  \param width   field width
  \returns string representation of `val`
*/

string FmtD(long val, int width) {
  char buf[80];
  (void)::snprintf(buf, sizeof(buf), "%*ld", width, val);
  return buf;
}

//-----------------------------------------------------------------------------
/*!
  \ingroup FormatHelper
  \brief Convert unsigned integer with %%u (decimal)
  \param val     value to be converted
  \param width   field width
  \returns string representation of `val`
*/

string FmtD(unsigned long val, int width) {
  char buf[80];
  (void)::snprintf(buf, sizeof(buf), "%*lu", width, val);
  return buf;
}

//-----------------------------------------------------------------------------
/*!
  \ingroup FormatHelper
  \brief Convert unsigned integer with %%x (hex)
  \param val     value to be converted
  \param width   field width
  \param prec    number of filled digits
  \returns string representation of `val`
*/

string FmtX(unsigned long val, int width, int prec) {
  char buf[80];
  (void)::snprintf(buf, sizeof(buf), "%*.*lx", width, prec, val);
  return buf;
}

//-----------------------------------------------------------------------------
/*!
  \ingroup FormatHelper
  \brief Convert unsigned integer (binary)
  \param val     value to be converted
  \param width   field width
  \returns string representation of `val`
*/

string FmtB(unsigned long val, int width) {
  string buf;
  while (val) {
    buf.insert(0, (val & 0x1) ? "1" : "0");
    val = val >> 1;
  }
  for (size_t i = buf.length(); int(i) < width; i++)
    buf.insert(0, "0");
  return buf;
}

//-----------------------------------------------------------------------------
/*!
  \ingroup FormatHelper
  \brief Convert double with %%f
  \param val     value to be converted
  \param width   field width
  \param prec    precision
  \returns string representation of `val`
*/

string FmtF(double val, int width, int prec) {
  char buf[80];
  (void)::snprintf(buf, sizeof(buf), "%*.*f", width, prec, val);
  return buf;
}

//-----------------------------------------------------------------------------
/*!
  \ingroup FormatHelper
  \brief Convert double with %%e
  \param val     value to be converted
  \param width   field width
  \param prec    precision
  \returns string representation of `val`
*/

string FmtE(double val, int width, int prec) {
  char buf[80];
  (void)::snprintf(buf, sizeof(buf), "%*.*e", width, prec, val);
  return buf;
}

} // end namespace cbm
