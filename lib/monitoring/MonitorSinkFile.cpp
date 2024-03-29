// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "MonitorSinkFile.hpp"

#include "Monitor.hpp"

#include "fmt/format.h"

#include <fstream>
#include <stdexcept>

namespace cbm {
using namespace std::string_literals; // for ""s

/*! \class MonitorSinkFile
  \brief Monitor sink - concrete sink for file output
*/

//-----------------------------------------------------------------------------
/*! \brief Constructor
  \param monitor back reference to Monitor
  \param path   filename

  Write metrics to a file named `path`. The special names `cout` and `cerr`
  will open these standard streams, in all other cases a file will be opened.

  Several MonitorSinkFile sinks with different `path` can be opened
  simultaneously and will receive the same data.
 */

MonitorSinkFile::MonitorSinkFile(Monitor& monitor, const std::string& path)
    : MonitorSink(monitor, path) {
  if (path == "cout"s) {
    fpCout = &std::cout;
  } else if (path == "cerr"s) {
    fpCout = &std::cerr;
  } else {
    fpOStream = std::make_unique<std::ofstream>(path);
    if (!fpOStream->is_open())
      throw std::runtime_error(fmt::format("MonitorSinkFile::ctor: open()"
                                           " failed for '{}'",
                                           path));
  }
}

//-----------------------------------------------------------------------------
/*! \brief Process a vector of metrics
 */

void MonitorSinkFile::ProcessMetricVec(const std::vector<Metric>& metvec) {
  std::ostream& os = fpCout ? *fpCout : *fpOStream;
  for (auto& met : metvec)
    os << InfluxLine(met) << "\n";
  if (size(metvec) > 0)
    os.flush();
}

//-----------------------------------------------------------------------------
/*! \brief Process heartbeat (noop for file sink)
 */

void MonitorSinkFile::ProcessHeartbeat() {}

} // end namespace cbm
