// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_MonitorSink
#define included_Cbm_MonitorSink 1

#include "Metric.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace cbm {

class Monitor; // forward declaration

class MonitorSink {
public:
  MonitorSink(Monitor& monitor, std::string path);
  virtual ~MonitorSink() = default;

  virtual void ProcessMetricVec(const std::vector<Metric>& metvec) = 0;
  virtual void ProcessHeartbeat() = 0;

protected:
  std::string CleanString(const std::string& id);
  std::string EscapeString(std::string_view str);
  std::string InfluxTags(const Metric& point);
  std::string InfluxFields(const Metric& point);
  std::string InfluxLine(const Metric& point);

  Monitor& fMonitor;       //!< back reference to Monitor
  std::string fSinkPath;   //!< path for output
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
