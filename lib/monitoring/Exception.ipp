// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

namespace cbm {
using namespace std;

//-----------------------------------------------------------------------------
//! \brief Returns what string std::string
inline const string& Exception::What() const noexcept { return fString; }

} // end namespace cbm
