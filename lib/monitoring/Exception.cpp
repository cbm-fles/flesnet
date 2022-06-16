// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "Exception.hpp"

namespace cbm {
using namespace std;

/*! \class Exception
  \brief Exception with a std::string as what() value
 */

//-----------------------------------------------------------------------------
/*! \brief Constructor
  \param str  will be used as what() string
 */

Exception::Exception(const string& str) : fString(str) {}

//-----------------------------------------------------------------------------
//! \brief Returns what string as const char* (std::exception interface)

const char* Exception::what() const noexcept { return fString.c_str(); }

} // end namespace cbm
