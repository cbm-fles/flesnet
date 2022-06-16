// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_MonitorSink
#define included_Cbm_MonitorSink 1

#include "Metric.hpp"

#include <string>
#include <vector>

namespace cbm {
using namespace std;

class Monitor; // forward declaration

class MonitorSink {
public:
  MonitorSink(Monitor& monitor, const string& path);
  virtual ~MonitorSink() = default;

  virtual void ProcessMetricVec(const vector<Metric>& metvec) = 0;
  virtual void ProcessHeartbeat() = 0;

protected:
  string CleanString(const string& id);
  string EscapeString(const string& str);
  string InfluxTags(const Metric& point);
  string InfluxFields(const Metric& point);
  string InfluxLine(const Metric& point);

protected:
  Monitor& fMonitor;       //!< back reference to Monitor
  string fSinkPath;        //!< path for output
  long fStatNPoint{0};     //!< # of processed points
  long fStatNTag{0};       //!< # of processed tags
  long fStatNField{0};     //!< # of processed fields
  long fStatNSend{0};      //!< # of send requests
  long fStatNByte{0};      //!< # of send bytes
  double fStatSndTime{0.}; //!< time spend in send requests
};

} // end namespace cbm

// #include "MonitorSink.ipp"

#endif
