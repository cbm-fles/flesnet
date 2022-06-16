// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "MonitorSink.hpp"

#include "ChronoHelper.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace cbm {
using namespace std;

/*! \class MonitorSink
  \brief Monitor sink - abstract base class

  This class provides an abstract interface for the Monitor sink layer.
  Concrete implementations are
  - MonitorSinkFile: concrete sink for file output (in InfluxDB line format)
  - MonitorSinkInflux1: concrete sink for InfluxDB V1 output
*/

//-----------------------------------------------------------------------------
/*! \brief Constructor
  \param monitor back reference to Monitor
  \param path    path for output
 */

MonitorSink::MonitorSink(Monitor& monitor, const string& path)
    : fMonitor(monitor), fSinkPath(path) {}

//-----------------------------------------------------------------------------
/*! \brief Removes protocol characters from a string
  \param str  input string
  \returns input with blank, '=', and ',' characters removed

  For simplicity and efficiency protocol meta characters are simply removed
  and not escaped. Tag and field key names as well as field values simply
  should never contain them.
 */

string MonitorSink::CleanString(const string& str) {
  string res = str;
  res.erase(remove(res.begin(), res.end(), ' '), res.end());
  res.erase(remove(res.begin(), res.end(), '='), res.end());
  res.erase(remove(res.begin(), res.end(), ','), res.end());
  return res;
}

//-----------------------------------------------------------------------------
/*! \brief Escapes string delimiter character '"' of a string
  \param str   input string
  \returns string with all '"' characters escaped with a backslash
 */

string MonitorSink::EscapeString(const string& str) {
  string res = str;
  size_t pos = 0;
  while ((pos = res.find('"', pos)) != string::npos) {
    res.replace(pos, 1, "\\\"");
    pos += 2;
  }
  return res;
}

//-----------------------------------------------------------------------------
/*! \brief Return tagset string for a Metric `point` in InfluxDB line format
 */

string MonitorSink::InfluxTags(const Metric& point) {
  string res;
  for (auto& tag : point.fTagset) {
    res += res.empty() ? "" : ",";
    res += CleanString(tag.first) + "=" + CleanString(tag.second);
  }
  return res;
}

//-----------------------------------------------------------------------------
/*! \brief Return field string for a Metric `point` in InfluxDB line format
 */

template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

string MonitorSink::InfluxFields(const Metric& point) {
  stringstream ss;
  ss.precision(16); // ensure full double precision
  for (auto& field : point.fFieldset) {
    if (ss.tellp() != 0)
      ss << ",";
    auto& key = field.first;
    auto& val = field.second;
    ss << CleanString(key) << "=";
    // use overloaded visitor pattern as described in cppreference.com
    visit(overloaded{[&ss](bool arg) { // case bool
                       ss << (arg ? "true" : "false");
                     },
                     [&ss](int arg) { // case int
                       ss << arg << "i";
                     },
                     [&ss](long arg) { // case long
                       ss << arg << "i";
                     },
                     [&ss](unsigned long arg) { // case unsigned long
                       ss << arg << "i";
                     },
                     [&ss](double arg) { // case double
                       ss << arg;
                     },
                     [this, &ss](const string& arg) { // case string
                       ss << '"' << EscapeString(arg) << '"';
                     }},
          val);
  }
  return ss.str();
}

//-----------------------------------------------------------------------------
/*! \brief Return Metric `point` in InfluxDB line format
 */

string MonitorSink::InfluxLine(const Metric& point) {
  string res = point.fMeasurement;
  res += "," + InfluxTags(point);
  res += " " + InfluxFields(point);
  res += " " + to_string(ScTimePoint2Nsec(point.fTimestamp));
  return res;
}

} // end namespace cbm
