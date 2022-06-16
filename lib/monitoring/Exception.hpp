// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_Exception
#define included_Cbm_Exception 1

#include <exception>
#include <string>

namespace cbm {
using namespace std;

class Exception : public exception {
public:
  Exception() = default;
  explicit Exception(const string& str);

  virtual const char* what() const noexcept;
  const string& What() const noexcept;

private:
  string fString{""}; //!< what string
};

} // end namespace cbm

#include "Exception.ipp"

#endif
