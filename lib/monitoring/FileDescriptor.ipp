// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

namespace cbm {

/*! \class FileDescriptor
  \brief Keeper for a linux file descriptor to be closed at destruct time
 */

//-----------------------------------------------------------------------------
//! \brief Destructor, will close the file descriptor it has been set

inline FileDescriptor::~FileDescriptor() { Close(); }

//-----------------------------------------------------------------------------
//! \brief Set file descriptor
//! \throws Exception in case descriptor has already been set

inline void FileDescriptor::Set(int fd) {
  if (fFd >= 0)
    throw Exception("FileDescriptor::Set(): already holding fd="s + FmtD(fFd));
  fFd = fd;
}

//-----------------------------------------------------------------------------
//! \brief Close and unset file descriptor

inline void FileDescriptor::Close() {
  if (fFd > 2)
    (void)::close(fFd);
  fFd = -1;
}

//-----------------------------------------------------------------------------
//! \brief int() conversion, allows to directly use an object like an integer

inline FileDescriptor::operator int() const { return fFd; }

} // end namespace cbm
