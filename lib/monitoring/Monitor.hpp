// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_Monitor
#define included_Cbm_Monitor 1

#include "ChronoDefs.hpp"
#include "FileDescriptor.hpp"
#include "Metric.hpp"
#include "MonitorSink.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cbm {
using namespace std;

class MonitorSink; // forward declaration

class Monitor {
public:
  Monitor();
  virtual ~Monitor();

  Monitor(const Monitor&) = delete;
  Monitor& operator=(const Monitor&) = delete;

  void OpenSink(const string& sname);
  void CloseSink(const string& sname);
  vector<string> SinkList();

  void QueueMetric(const Metric& point);
  void QueueMetric(Metric&& point);
  void QueueMetric(const string& measurement,
                   const MetricTagSet& tagset,
                   const MetricFieldSet& fieldset,
                   sctime_point timestamp = sctime_point());
  void QueueMetric(const string& measurement,
                   const MetricTagSet& tagset,
                   MetricFieldSet&& fieldset,
                   sctime_point timestamp = sctime_point());
  void QueueMetric(const string& measurement,
                   MetricTagSet&& tagset,
                   MetricFieldSet&& fieldset,
                   sctime_point timestamp = sctime_point());
  const string& HostName() const;

  static Monitor& Ref();
  static Monitor* Ptr();

public:
  // some constants
  static const int kELoopTimeout = 10000; //!< monitor flush time in ms

private:
  void Stop();
  void Wakeup();
  void EventLoop();
  MonitorSink& SinkRef(const string& sname);

private:
  using metvec_t = vector<Metric>;
  using sink_uptr_t = unique_ptr<MonitorSink>;
  using smap_t = unordered_map<string, sink_uptr_t>;

  FileDescriptor fEvtFd{};       //!< fd for eventfd file
  thread fThread{};              //!< worker thread
  metvec_t fMetVec{};            //!< metric list
  mutex fMetVecMutex{};          //!< mutex for fMetVec access
  string fHostName{""};          //!< hostname
  bool fStopped{false};          //!< signals thread rundown
  smap_t fSinkMap{};             //!< sink registry
  mutex fSinkMapMutex{};         //!< mutex for fSinkMap access
  sctime_point fNextHeartbeat{}; //!< time of next heartbeat
  static Monitor* fpSingleton;   //!< \glos{singleton} this
};

} // end namespace cbm

#include "Monitor.ipp"

#endif
