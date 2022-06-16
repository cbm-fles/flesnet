// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "Monitor.hpp"

#include "ChronoHelper.hpp"
#include "Exception.hpp"
#include "MonitorSinkFile.hpp"
#include "MonitorSinkInflux1.hpp"
#include "MonitorSinkInflux2.hpp"
#include "PThreadHelper.hpp"
#include "SysCallException.hpp"

#include "fmt/format.h"

#include <errno.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace cbm {
using namespace std;
using namespace std::chrono_literals;

/*! \class Monitor
  \brief Thread-safe metric monitor system for CBM

  This class provides a thread-safe metric monitor for the CBM framework.
  The Monitor is instantiated as a \glos{singleton} in Context right after
  Logger is up and is destroyed as 2nd last step prior to Logger, and is
  therefore available for \glos{DObject}s in a \glos{CBMmain} throughout
  the whole lifetime.

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

  The Monitor back-end is provided by MonitorSink objects and controlled via
  - OpenSink(): creates a new sink
  - CloseSink(): removes a sink

  Currently two sink type is implemented
  - MonitorSinkFile: writes to files
  - MonitorSinkInflux1: writes to a InfluxDB V1.x time-series database
  - MonitorSinkInflux2: writes to a InfluxDB V2.x time-series database

  The Monitor is a \glos{singleton} and accessed via the Monitor::Ref() static
  method.

  \note On \ref objectownership
    - is owned by Context

  \note **Implementation notes**
  - the Monitor uses a worker thread named "Cbm:monitor" and a `mutex` to
    protect the metric queue. The code sequences executed under `mutex` lock
    use `std::move` and are absolutely minimal, contention when locking the
    `mutex` is thus very unlikely:
    - at metrics queueing: just a `vector::push_back(move(...))`
    - at metrics processing: just a `vector::swap(...)`
*/

//-----------------------------------------------------------------------------
// some constants
static constexpr scduration kHeartbeat = 60s; // heartbeat interval

//-----------------------------------------------------------------------------
/*! \brief Constructor
  \throws Exception in case Monitor is already instantiated
  \throws SysCallException in case a system calls fails

  Initializes the Monitor \glos{singleton} and creates a work thread with
  the name "Cbm:monitor" for processing the metrics.
 */

Monitor::Monitor() {
  // singleton check
  if (fpSingleton)
    throw Exception("Monitor::ctor: already instantiated");

  // setup eventfd
  int fd = ::eventfd(0U, 0);
  if (fd < 0)
    throw SysCallException("Monitor::ctor"s, "eventfd"s, errno);
  fEvtFd.Set(fd);

  // get hostname
  char hostname[80];
  if (int rc = ::gethostname(hostname, sizeof(hostname)); rc < 0)
    throw SysCallException("Monitor::ctor"s, "gethostname"s, errno);
  fHostName = hostname;

  // init heartbeat sequence
  fNextHeartbeat = ScNow();

  // start EventLoop
  fThread = thread([this]() { EventLoop(); });

  fpSingleton = this;
}

//-----------------------------------------------------------------------------
/*! \brief Destructor

  Calls Stop(), which will trigger the processing of all still pending
  metrics and termination of the work thread.
 */

Monitor::~Monitor() {
  fpSingleton = nullptr;
  Stop();
}

//-----------------------------------------------------------------------------
/*! \brief Open a new sink
  \param sname    sink name, given as proto:path

  `sname` must have the form `proto:path`. Currently supported `proto` values
  - `file`: will create a MonitorSinkFile sink
  - `influx1`: will create a MonitorSinkInflux1 sink
  - `influx2`: will create a MonitorSinkInflux2 sink
 */

void Monitor::OpenSink(const string& sname) {

  {
    lock_guard<mutex> lock(fSinkMapMutex);
    auto it = fSinkMap.find(sname);
    if (it != fSinkMap.end())
      throw Exception(
          fmt::format("Monitor::OpenSink: sink '{}' already open", sname));
  }

  auto pos = sname.find(':');
  if (pos == string::npos)
    throw Exception(fmt::format("Monitor::OpenSink:"
                                " no sink type specified in '{}'",
                                sname));

  string stype = sname.substr(0, pos);
  string spath = sname.substr(pos + 1);

  if (stype == "file") {
    unique_ptr<MonitorSink> uptr = make_unique<MonitorSinkFile>(*this, spath);
    lock_guard<mutex> lock(fSinkMapMutex);
    fSinkMap.try_emplace(sname, move(uptr));
  } else if (stype == "influx1") {
    unique_ptr<MonitorSink> uptr =
        make_unique<MonitorSinkInflux1>(*this, spath);
    lock_guard<mutex> lock(fSinkMapMutex);
    fSinkMap.try_emplace(sname, move(uptr));
  } else if (stype == "influx2") {
    unique_ptr<MonitorSink> uptr =
        make_unique<MonitorSinkInflux2>(*this, spath);
    lock_guard<mutex> lock(fSinkMapMutex);
    fSinkMap.try_emplace(sname, move(uptr));
  } else {
    throw Exception(
        fmt::format("Monitor::OpenSink: invalid sink type '{}'", stype));
  }
}

//-----------------------------------------------------------------------------
/*! \brief Close a sink
  \param sname    sink name, given as proto:path
  \throws Exception if no sink named `sname` exists
 */

void Monitor::CloseSink(const string& sname) {
  lock_guard<mutex> lock(fSinkMapMutex);
  if (fSinkMap.erase(sname) == 0)
    throw Exception(
        fmt::format("Monitor::CloseSink: sink '{}' not found", sname));
}

//-----------------------------------------------------------------------------
//! \brief Return list of all sinks

vector<string> Monitor::SinkList() {
  vector<string> res;
  lock_guard<mutex> lock(fSinkMapMutex);
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
  QueueMetric(move(tmp));
}

//-----------------------------------------------------------------------------
/*! \brief Queues a metric point (move semantics)
  \param point  Metric object, will be `move`ed to the metric queue
 */

void Monitor::QueueMetric(Metric&& point) {
  if (fStopped)
    return; // discard when already stopped
  auto ts = point.fTimestamp;
  if (ts == sctime_point())
    ts = ScNow();
  {
    lock_guard<mutex> lock(fMetVecMutex);
    fMetVec.emplace_back(move(point));
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

void Monitor::QueueMetric(const string& measurement,
                          const MetricTagSet& tagset,
                          const MetricFieldSet& fieldset,
                          sctime_point timestamp) {
  Metric point(measurement, tagset, fieldset, timestamp);
  QueueMetric(move(point));
}

//-----------------------------------------------------------------------------
/*! \brief Queues a metric point
  \param measurement  measurement id
  \param tagset       set of tags
  \param fieldset     set of fields (will be moved)
  \param timestamp    timestamp (defaults to now() when omitted)
 */

void Monitor::QueueMetric(const string& measurement,
                          const MetricTagSet& tagset,
                          MetricFieldSet&& fieldset,
                          sctime_point timestamp) {
  Metric point(measurement, tagset, move(fieldset), timestamp);
  QueueMetric(move(point));
}

//-----------------------------------------------------------------------------
/*! \brief Queues a metric point
  \param measurement  measurement id
  \param tagset       set of tags (will be moved)
  \param fieldset     set of fields (will be moved)
  \param timestamp    timestamp (defaults to now() when omitted)
 */

void Monitor::QueueMetric(const string& measurement,
                          MetricTagSet&& tagset,
                          MetricFieldSet&& fieldset,
                          sctime_point timestamp) {
  Metric point(measurement, move(tagset), move(fieldset), timestamp);
  QueueMetric(move(point));
}

//-----------------------------------------------------------------------------
/*! \brief Stop Monitor work thread

  Calls Wakeup() to wakeup the work thread. This triggers the processing
  of all still pending metrics, after that the thread will terminate.
  Stop() joins the work thread, after that the Monitor object can be
  safely destructed.
 */

void Monitor::Stop() {
  fStopped = true;
  Wakeup();
  if (fThread.joinable())
    fThread.join();
}

//-----------------------------------------------------------------------------
/*! \brief Wakeup work thread

  Uses an `eventfd` channel to wakeup the work thread. Is used to trigger
  the processing of metrics, e.g. for thread shutdown with Stop().
 */

void Monitor::Wakeup() {
  uint64_t one(1);
  if (::write(fEvtFd, &one, sizeof(one)) != sizeof(one))
    throw SysCallException("Monitor::Wakeup"s, "write"s, "fEvtFd"s, errno);
}

//-----------------------------------------------------------------------------
/*! \brief The event loop of Monitor work thread
 */

void Monitor::EventLoop() {
  using namespace std::chrono_literals;

  // set worker thread name, for convenience (e.g. for top 'H' display)
  SetPThreadName("Cbm:monitor");

  pollfd polllist[1];
  polllist[0] = pollfd{fEvtFd, POLLIN, 0};

  while (true) {
    ::poll(polllist, 1, kELoopTimeout); // timeout results in auto flush

    // handle fEvtFd -------------------------------------------------
    if (polllist[0].revents == POLLIN) {
      uint64_t cnt = 0;
      if (::read(fEvtFd, &cnt, sizeof(cnt)) != sizeof(cnt))
        throw SysCallException("Monitor::EventLoop"s, "read"s, "fEvtFd"s,
                               errno);
    }

    metvec_t metvec;
    {
      lock_guard<mutex> lock(fMetVecMutex);
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
      lock_guard<mutex> lock(fSinkMapMutex);
      for (auto& kv : fSinkMap)
        (*kv.second).ProcessMetricVec(metvec);
    }

    if (ScNow() > fNextHeartbeat && !fStopped) { // handle heartbeats
      fNextHeartbeat += kHeartbeat;              // schedule nexr
      lock_guard<mutex> lock(fSinkMapMutex);
      for (auto& kv : fSinkMap)
        (*kv.second).ProcessHeartbeat();
    }

    if (fStopped)
      break;
  } // while (true)
}

//-----------------------------------------------------------------------------
/*! \brief Returns reference to a sink
  \param sname    sink name, given as proto:path
  \throws Exception if no sink named `sname` exists
  \returns MonitorSink reference
 */

MonitorSink& Monitor::SinkRef(const string& sname) {
  auto it = fSinkMap.find(sname);
  if (it == fSinkMap.end())
    throw Exception(
        fmt::format("Monitor::SinkRef: sink '{}' not found", sname));
  return *(it->second.get());
}

//-----------------------------------------------------------------------------
// define static member variables

Monitor* Monitor::fpSingleton = nullptr;

} // end namespace cbm
