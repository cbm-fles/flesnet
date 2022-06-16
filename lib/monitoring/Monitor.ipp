// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

namespace cbm {

//-----------------------------------------------------------------------------
//! \brief Returns hostname used by Monitor

inline const string& Monitor::HostName() const { return fHostName; }

//-----------------------------------------------------------------------------
//! \brief Static method which returns a reference of the
//!  Monitor \glos{singleton}

inline Monitor& Monitor::Ref() { return *fpSingleton; }

//-----------------------------------------------------------------------------
//! \brief Static method which returns a pointer to the
//!  Monitor \glos{singleton}

inline Monitor* Monitor::Ptr() { return fpSingleton; }

} // end namespace cbm
