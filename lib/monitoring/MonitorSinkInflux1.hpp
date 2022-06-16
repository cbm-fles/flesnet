// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_MonitorSinkInflux1
#define included_Cbm_MonitorSinkInflux1 1

#include "MonitorSink.hpp"

namespace cbm {
using namespace std;

class MonitorSinkInflux1 : public MonitorSink {
public:
  MonitorSinkInflux1(Monitor& monitor, const string& path);

  virtual void ProcessMetricVec(const vector<Metric>& metvec);
  virtual void ProcessHeartbeat();

private:
  void SendData(const string& msg);

private:
  string fHost; //!< server host name
  string fPort; //!< port for InfluxDB
  string fDB;   //!< target database
};

} // end namespace cbm

//#include "MonitorSinkInflux1.ipp"

#endif
