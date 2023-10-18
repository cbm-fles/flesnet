// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "Monitor.hpp"

#include "MonitorSinkFile.hpp"
#include "MonitorSinkInflux1.hpp"
#include "MonitorSinkInflux2.hpp"
#include "System.hpp"
#include <stdexcept>

#include "fmt/format.h"

namespace cbm {

/*! \class Monitor
  \brief Thread-safe metric monitor system for CBM

  This class provides a thread-safe metric monitor for the CBM framework.

  The Monitor system has two layers
  - a core which collects and buffers metrics
  - back-ends called `sinks` which write metrics to time-series data bases

  The Monitor processes data provided in a Metric object with four parts
  - a measurement identifier
  - a tag list
  - a field list
  - a timestamp

  Several overloaded versions of the QueueMetric() method allow to queue a
  Metric for further processing by tne Monitor core. Because Monitor is a
  \glos{singleton} the typical usage looks like
  \code{.cpp}
   Monitor::Ref().QueueMetric("TesterTop",             // measurement
                              {{"oid", ObjectId()},    // tags
                               {"wid", WorkerId()}},
                              {{"dt", dt},             // fields
                               {"ndone", ndone},
                               {"go", nrepeat>0}});
  \endcode

  See QueueMetric() for a more detailed description the Monitor input interface.

  The Monitor back end is provided by MonitorSink objects and controlled via
  - OpenSink(): creates a new sink
  - CloseSink(): removes a sink

  Currently two sink types are implemented
  - MonitorSinkFile: writes to files
  - MonitorSinkInflux1: writes to an InfluxDB V1.x time-series database
  - MonitorSinkInflux2: writes to an InfluxDB V2.x time-series database

  The Monitor is a \glos{singleton} and accessed via the Monitor::Ref() static
  method.

  \note **Implementation notes**
  - the Monitor uses a worker thread named "cbm:monitor" and a `mutex` to
    protect the metric queue. The code sequences executed under `mutex` lock
    use `std::move` and are absolutely minimal, contention when locking the
    `mutex` is thus very unlikely:
    - at metrics queueing: just a `vector::push_back(move(...))`
    - at metrics processing: just a `vector::swap(...)`
*/

//-----------------------------------------------------------------------------
/*! \brief Constructor
  \throws std::runtime_error in case Monitor is already instantiated or a system
  call fails

  Initializes the Monitor \glos{singleton} and creates a work thread with
  the name "cbm:monitor" for processing the metrics.
 */

Monitor::Monitor(const std::string& sname) {
  // singleton check
  if (fpSingleton)
    throw std::runtime_error("Monitor::ctor: already instantiated");

  // get hostname
  fHostName = cbm::system::current_hostname();

  // init heartbeat sequence
  fNextHeartbeat = std::chrono::system_clock::now();

  // start EventLoop
  fThread = std::thread([this]() { EventLoop(); });

  fpSingleton = this;

  if (!sname.empty()) {
    OpenSink(sname);
  }
}

//-----------------------------------------------------------------------------
/*! \brief Destructor

  Trigger the processing of all still pending metrics and termination of the
  work thread.
 */

Monitor::~Monitor() {
  fpSingleton = nullptr;

  // Set fStopped and wake up worker thread. This triggers the processing of all
  // still pending metrics, after that the thread will terminate.
  {
    std::lock_guard<std::mutex> lk(fControlMutex);
    fStopped = true;
  }
  fControlCV.notify_one();

  if (fThread.joinable())
    fThread.join();
}

//-----------------------------------------------------------------------------
/*! \brief Open a new sink
  \param sname    sink name, given as proto:path

  `sname` must have the form `proto:path`. Currently supported `proto` values
  - `file`: will create a MonitorSinkFile sink
  - `influx1`: will create a MonitorSinkInflux1 sink
  - `influx2`: will create a MonitorSinkInflux2 sink
 */

void Monitor::OpenSink(const std::string& sname) {

  {
    std::lock_guard<std::mutex> lock(fSinkMapMutex);
    auto it = fSinkMap.find(sname);
    if (it != fSinkMap.end())
      throw std::runtime_error(
          fmt::format("Monitor::OpenSink: sink '{}' already open", sname));
  }

  auto pos = sname.find(':');
  if (pos == std::string::npos)
    throw std::runtime_error(fmt::format("Monitor::OpenSink:"
                                         " no sink type specified in '{}'",
                                         sname));

  std::string stype = sname.substr(0, pos);
  std::string spath = sname.substr(pos + 1);

  if (stype == "file") {
    std::unique_ptr<MonitorSink> uptr =
        std::make_unique<MonitorSinkFile>(*this, spath);
    std::lock_guard<std::mutex> lock(fSinkMapMutex);
    fSinkMap.try_emplace(sname, std::move(uptr));
  } else if (stype == "influx1") {
    std::unique_ptr<MonitorSink> uptr =
        std::make_unique<MonitorSinkInflux1>(*this, spath);
    std::lock_guard<std::mutex> lock(fSinkMapMutex);
    fSinkMap.try_emplace(sname, std::move(uptr));
  } else if (stype == "influx2") {
    std::unique_ptr<MonitorSink> uptr =
        std::make_unique<MonitorSinkInflux2>(*this, spath);
    std::lock_guard<std::mutex> lock(fSinkMapMutex);
    fSinkMap.try_emplace(sname, std::move(uptr));
  } else {
    throw std::runtime_error(
        fmt::format("Monitor::OpenSink: invalid sink type '{}'", stype));
  }
}

//-----------------------------------------------------------------------------
/*! \brief Close a sink
  \param sname    sink name, given as proto:path
  \throws std::runtime_error if no sink named `sname` exists
 */

void Monitor::CloseSink(const std::string& sname) {
  std::lock_guard<std::mutex> lock(fSinkMapMutex);
  if (fSinkMap.erase(sname) == 0)
    throw std::runtime_error(
        fmt::format("Monitor::CloseSink: sink '{}' not found", sname));
}

//-----------------------------------------------------------------------------
//! \brief Return list of all sinks

std::vector<std::string> Monitor::SinkList() {
  std::vector<std::string> res;
  std::lock_guard<std::mutex> lock(fSinkMapMutex);
  for (auto& kv : fSinkMap)
    res.push_back(kv.first);
  return res;
}

//-----------------------------------------------------------------------------
/*! \brief Queues a metric point (copy semantics)
  \param point  Metric object, will be copied to the metric queue
 */

void Monitor::QueueMetric(const Metric& point) {
  auto tmp = point;
  QueueMetric(std::move(tmp));
}

//-----------------------------------------------------------------------------
/*! \brief Queues a metric point (move semantics)
  \param point  Metric object, will be `move`ed to the metric queue
 */

void Monitor::QueueMetric(Metric&& point) {
  if (fStopped)
    return; // discard when already stopped
  auto ts = point.fTimestamp;
  if (ts == time_point())
    ts = std::chrono::system_clock::now();
  {
    std::lock_guard<std::mutex> lock(fMetVecMutex);
    fMetVec.emplace_back(std::move(point));
    fMetVec[fMetVec.size() - 1].fTimestamp = ts;
  }
}

//-----------------------------------------------------------------------------
/*! \brief Queues a metric point
  \param measurement  measurement id
  \param tagset       set of tags
  \param fieldset     set of fields
  \param timestamp    timestamp (defaults to now() when omitted)
 */

void Monitor::QueueMetric(const std::string& measurement,
                          const MetricTagSet& tagset,
                          const MetricFieldSet& fieldset,
                          time_point timestamp) {
  Metric point(measurement, tagset, fieldset, timestamp);
  QueueMetric(std::move(point));
}

//-----------------------------------------------------------------------------
/*! \brief Queues a metric point
  \param measurement  measurement id
  \param tagset       set of tags
  \param fieldset     set of fields (will be moved)
  \param timestamp    timestamp (defaults to now() when omitted)
 */

void Monitor::QueueMetric(const std::string& measurement,
                          const MetricTagSet& tagset,
                          MetricFieldSet&& fieldset,
                          time_point timestamp) {
  Metric point(measurement, tagset, std::move(fieldset), timestamp);
  QueueMetric(std::move(point));
}

//-----------------------------------------------------------------------------
/*! \brief Queues a metric point
  \param measurement  measurement id
  \param tagset       set of tags (will be moved)
  \param fieldset     set of fields (will be moved)
  \param timestamp    timestamp (defaults to now() when omitted)
 */

void Monitor::QueueMetric(const std::string& measurement,
                          MetricTagSet&& tagset,
                          MetricFieldSet&& fieldset,
                          time_point timestamp) {
  Metric point(measurement, std::move(tagset), std::move(fieldset), timestamp);
  QueueMetric(std::move(point));
}

//-----------------------------------------------------------------------------
/*! \brief The event loop of Monitor work thread
 */

void Monitor::EventLoop() {
  // set worker thread name, for convenience (e.g. for top 'H' display)
  cbm::system::set_thread_name("cbm:monitor");

  bool stopped = false;
  while (!stopped) {
    std::unique_lock lk(fControlMutex);
    stopped =
        fControlCV.wait_for(lk, kELoopTimeout, [this] { return fStopped; });
    // timeout results in auto flush

    metvec_t metvec;
    {
      std::lock_guard<std::mutex> lock(fMetVecMutex);
      if (!fMetVec.empty()) {
        // move whole vector from protected queue to local environment
        metvec.swap(fMetVec);
        // determine sensible capacity to minimize re-allocs
        size_t ncap = metvec.capacity();
        if (metvec.size() > metvec.capacity() / 2)
          ncap += ncap / 2;
        else
          ncap = ncap / 2;
        if (ncap < 4)
          ncap = 4;
        fMetVec.reserve(ncap);
      }
    }

    if (metvec.size() > 0) {
      std::lock_guard<std::mutex> lock(fSinkMapMutex);
      for (auto& kv : fSinkMap)
        (*kv.second).ProcessMetricVec(metvec);
    }
    if (std::chrono::system_clock::now() > fNextHeartbeat && !stopped) {
      // handle heartbeats
      fNextHeartbeat += kHeartbeat; // schedule next
      std::lock_guard<std::mutex> lock(fSinkMapMutex);
      for (auto& kv : fSinkMap)
        (*kv.second).ProcessHeartbeat();
    }
  }
}

//-----------------------------------------------------------------------------
/*! \brief Returns reference to a sink
  \param sname    sink name, given as proto:path
  \throws std::runtime_error if no sink named `sname` exists
  \returns MonitorSink reference
 */

MonitorSink& Monitor::SinkRef(const std::string& sname) {
  auto it = fSinkMap.find(sname);
  if (it == fSinkMap.end())
    throw std::runtime_error(
        fmt::format("Monitor::SinkRef: sink '{}' not found", sname));
  return *(it->second.get());
}

//-----------------------------------------------------------------------------
// define static member variables

Monitor* Monitor::fpSingleton = nullptr;

} // end namespace cbm
