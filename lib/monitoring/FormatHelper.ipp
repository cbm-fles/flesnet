// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

namespace cbm {
using namespace std;

//-----------------------------------------------------------------------------
//! \ingroup FormatHelper
//! \brief Convert integer with %%d - forwarder to FmtD(long,int)

inline string FmtD(int val, int width) {
  return FmtD(static_cast<long>(val), width);
}

//-----------------------------------------------------------------------------
//! \ingroup FormatHelper
//! \brief Convert integer with %%d - forwarder to FmtD(long,int)

inline string FmtD(short val, int width) {
  return FmtD(static_cast<long>(val), width);
}

//-----------------------------------------------------------------------------
//! \ingroup FormatHelper
//! \brief Convert integer with %%d - forwarder to FmtD(long,int)

inline string FmtD(char val, int width) {
  return FmtD(static_cast<long>(val), width);
}

//-----------------------------------------------------------------------------
//! \ingroup FormatHelper
//! \brief Convert integer with %%d - forwarder to FmtD(unsigned long,int)

inline string FmtD(unsigned int val, int width) {
  return FmtD(static_cast<unsigned long>(val), width);
}

//-----------------------------------------------------------------------------
//! \ingroup FormatHelper
//! \brief Convert integer with %%d - forwarder to FmtD(unsigned long,int)

inline string FmtD(unsigned short val, int width) {
  return FmtD(static_cast<unsigned long>(val), width);
}

//-----------------------------------------------------------------------------
//! \ingroup FormatHelper
//! \brief Convert integer with %%d - forwarder to FmtD(unsigned long,int)

inline string FmtD(unsigned char val, int width) {
  return FmtD(static_cast<unsigned long>(val), width);
}

} // end namespace cbm
