// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_MonitorSinkFile
#define included_Cbm_MonitorSinkFile 1

#include "MonitorSink.hpp"

#include <fstream>
#include <iostream>
#include <memory>

namespace cbm {
using namespace std;

class MonitorSinkFile : public MonitorSink {
public:
  MonitorSinkFile(Monitor& monitor, const string& path);

  virtual void ProcessMetricVec(const vector<Metric>& metvec);
  virtual void ProcessHeartbeat();

private:
  ostream* fpCout{nullptr};         //!< pointer to cout/cerr
  unique_ptr<ofstream> fpOStream{}; //!< uptr to general output stream
};

} // end namespace cbm

//#include "MonitorSinkFile.ipp"

#endif
