// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_FileDescriptor
#define included_Cbm_FileDescriptor 1

#include "Exception.hpp"
#include "FormatHelper.hpp"

#include <unistd.h>

namespace cbm {

class FileDescriptor {
public:
  FileDescriptor() = default;
  ~FileDescriptor();

  void Set(int fd);
  void Close();
  operator int() const;

private:
  int fFd{-1}; //!< file descriptor number
};

} // end namespace cbm

#include "FileDescriptor.ipp"

#endif
