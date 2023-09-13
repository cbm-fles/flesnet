// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#include "MonitorSink.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>

namespace cbm {

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

MonitorSink::MonitorSink(Monitor& monitor, std::string path)
    : fMonitor(monitor), fSinkPath(std::move(path)) {}

//-----------------------------------------------------------------------------
/*! \brief Removes protocol characters from a string
  \param str  input string
  \returns input with blank, '=', and ',' characters removed

  For simplicity and efficiency protocol meta characters are simply removed
  and not escaped. Tag and field key names as well as field values simply
  should never contain them.
 */

std::string MonitorSink::CleanString(const std::string& str) {
  std::string res = str;
  res.erase(std::remove(res.begin(), res.end(), ' '), res.end());
  res.erase(std::remove(res.begin(), res.end(), '='), res.end());
  res.erase(std::remove(res.begin(), res.end(), ','), res.end());
  return res;
}

//-----------------------------------------------------------------------------
/*! \brief Escapes string delimiter character '"' of a string
  \param str   input string
  \returns string with all '"' characters escaped with a backslash
 */

std::string MonitorSink::EscapeString(std::string_view str) {
  std::string res(str);
  size_t pos = 0;
  while ((pos = res.find('"', pos)) != std::string::npos) {
    res.replace(pos, 1, "\\\"");
    pos += 2;
  }
  return res;
}

//-----------------------------------------------------------------------------
/*! \brief Return tagset string for a Metric `point` in InfluxDB line format
 */

std::string MonitorSink::InfluxTags(const Metric& point) {
  std::string res;
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

std::string MonitorSink::InfluxFields(const Metric& point) {
  std::stringstream ss;
  ss.precision(16); // ensure full double precision
  for (auto& field : point.fFieldset) {
    if (ss.tellp() != 0)
      ss << ",";
    auto& key = field.first;
    auto& val = field.second;
    ss << CleanString(key) << "=";
    // use overloaded visitor pattern as described in cppreference.com
    visit(overloaded{
              [&ss](bool arg) { // case bool
                ss << (arg ? "true" : "false");
              },
              [&ss](int32_t arg) { // case int32_t
                ss << arg << "i";
              },
              [&ss](uint32_t arg) { // case uint32_t
                ss << arg << "i";
              },
              [&ss](int64_t arg) { // case int64_t
                ss << arg << "i";
              },
              [&ss](uint64_t arg) { // case uint64_t
                ss << arg << "i";
              },
              [&ss](float arg) { // case float
                ss << arg;
              },
              [&ss](double arg) { // case double
                ss << arg;
              },
              [this, &ss](std::string_view arg) { // case string + string_view
                ss << '"' << EscapeString(arg) << '"';
              }},
          val);
  }
  return ss.str();
}

//-----------------------------------------------------------------------------
/*! \brief Return Metric `point` in InfluxDB line format
 */

std::string MonitorSink::InfluxLine(const Metric& point) {
  std::chrono::duration<long, std::nano> timestamp_ns =
      point.fTimestamp - std::chrono::system_clock::time_point();

  std::string res = point.fMeasurement;
  res += "," + InfluxTags(point);
  res += " " + InfluxFields(point);
  res += " " + std::to_string(timestamp_ns.count());
  return res;
}

} // end namespace cbm
