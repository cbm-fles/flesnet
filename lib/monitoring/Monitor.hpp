// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_Monitor
#define included_Cbm_Monitor 1

#include "Metric.hpp"
#include "MonitorSink.hpp"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cbm {
using namespace std;
using namespace std::chrono_literals;
using sc = chrono::system_clock;

class MonitorSink; // forward declaration

class Monitor {
public:
  explicit Monitor(const string& sname = "");
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
                   sc::time_point timestamp = sc::time_point());
  void QueueMetric(const string& measurement,
                   const MetricTagSet& tagset,
                   MetricFieldSet&& fieldset,
                   sc::time_point timestamp = sc::time_point());
  void QueueMetric(const string& measurement,
                   MetricTagSet&& tagset,
                   MetricFieldSet&& fieldset,
                   sc::time_point timestamp = sc::time_point());
  const string& HostName() const;

  static Monitor& Ref();
  static Monitor* Ptr();

public:
  // some constants
  static constexpr auto kELoopTimeout = 10s; //!< monitor flush time
  static constexpr auto kHeartbeat = 60s;    //!< heartbeat interval

private:
  void EventLoop();
  MonitorSink& SinkRef(const string& sname);

private:
  using metvec_t = vector<Metric>;
  using sink_uptr_t = unique_ptr<MonitorSink>;
  using smap_t = unordered_map<string, sink_uptr_t>;

  thread fThread{};                   //!< worker thread
  std::condition_variable fControlCV; //!< condition variable for thread control
  std::mutex fControlMutex{};         //!< mutex for thread control
  bool fStopped{false};               //!< signals thread rundown

  metvec_t fMetVec{};              //!< metric list
  mutex fMetVecMutex{};            //!< mutex for fMetVec access
  string fHostName{""};            //!< hostname
  smap_t fSinkMap{};               //!< sink registry
  mutex fSinkMapMutex{};           //!< mutex for fSinkMap access
  sc::time_point fNextHeartbeat{}; //!< time of next heartbeat
  static Monitor* fpSingleton;     //!< \glos{singleton} this
};

} // end namespace cbm

#include "Monitor.ipp"

#endif
