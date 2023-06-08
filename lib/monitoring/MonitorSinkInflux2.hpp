// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_MonitorSinkInflux2
#define included_Cbm_MonitorSinkInflux2 1

#include "MonitorSink.hpp"

namespace cbm {
class MonitorSinkInflux2 : public MonitorSink {
public:
  MonitorSinkInflux2(Monitor& monitor, const std::string& path);

  virtual void ProcessMetricVec(const std::vector<Metric>& metvec);
  virtual void ProcessHeartbeat();

private:
  void SendData(const std::string& msg);

private:
  std::string fHost;   //!< server host name
  std::string fPort;   //!< port for InfluxDB
  std::string fBucket; //!< target bucket
  std::string fToken;  //!< access token
};

} // end namespace cbm

// #include "MonitorSinkInflux2.ipp"

#endif
