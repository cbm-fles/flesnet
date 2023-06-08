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

class MonitorSink; // forward declaration

class Monitor {
public:
  using time_point = std::chrono::system_clock::time_point;

  explicit Monitor(const std::string& sname = "");
  virtual ~Monitor();

  Monitor(const Monitor&) = delete;
  Monitor& operator=(const Monitor&) = delete;

  void OpenSink(const std::string& sname);
  void CloseSink(const std::string& sname);
  std::vector<std::string> SinkList();

  void QueueMetric(const Metric& point);
  void QueueMetric(Metric&& point);
  void QueueMetric(const std::string& measurement,
                   const MetricTagSet& tagset,
                   const MetricFieldSet& fieldset,
                   time_point timestamp = time_point());
  void QueueMetric(const std::string& measurement,
                   const MetricTagSet& tagset,
                   MetricFieldSet&& fieldset,
                   time_point timestamp = time_point());
  void QueueMetric(const std::string& measurement,
                   MetricTagSet&& tagset,
                   MetricFieldSet&& fieldset,
                   time_point timestamp = time_point());
  const std::string& HostName() const;

  static Monitor& Ref();
  static Monitor* Ptr();

public:
  // some constants
  static constexpr auto kELoopTimeout =
      std::chrono::seconds(10); //!< monitor flush time
  static constexpr auto kHeartbeat =
      std::chrono::seconds(60); //!< heartbeat interval

private:
  void EventLoop();
  MonitorSink& SinkRef(const std::string& sname);

private:
  using metvec_t = std::vector<Metric>;
  using sink_uptr_t = std::unique_ptr<MonitorSink>;
  using smap_t = std::unordered_map<std::string, sink_uptr_t>;

  std::thread fThread{};              //!< worker thread
  std::condition_variable fControlCV; //!< condition variable for thread control
  std::mutex fControlMutex{};         //!< mutex for thread control
  bool fStopped{false};               //!< signals thread rundown

  metvec_t fMetVec{};          //!< metric list
  std::mutex fMetVecMutex{};   //!< mutex for fMetVec access
  std::string fHostName{""};   //!< hostname
  smap_t fSinkMap{};           //!< sink registry
  std::mutex fSinkMapMutex{};  //!< mutex for fSinkMap access
  time_point fNextHeartbeat{}; //!< time of next heartbeat
  static Monitor* fpSingleton; //!< \glos{singleton} this
};

} // end namespace cbm

#include "Monitor.ipp"

#endif
