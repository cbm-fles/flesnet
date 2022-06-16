// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_Metric
#define included_Cbm_Metric 1

#include "ChronoDefs.hpp"

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cbm {
using namespace std;

using MetricTagSet = vector<pair<string, string>>;
using MetricField = variant<bool, int, long, unsigned long, double, string>;
using MetricFieldSet = vector<pair<string, MetricField>>;

struct Metric {
  Metric() = default;
  Metric(const Metric&) = default;
  Metric(Metric&&) = default;
  Metric(const string& measurement,
         const MetricTagSet& tagset,
         const MetricFieldSet& fieldset,
         sctime_point timestamp = sctime_point());
  Metric(const string& measurement,
         const MetricTagSet& tagset,
         MetricFieldSet&& fieldset,
         sctime_point timestamp = sctime_point());
  Metric(const string& measurement,
         MetricTagSet&& tagset,
         MetricFieldSet&& fieldset,
         sctime_point timestamp = sctime_point());
  Metric& operator=(const Metric&) = default;
  Metric& operator=(Metric&&) = default;

  string fMeasurement{""};  //!< measurement name
  MetricTagSet fTagset;     //!< set of tags
  MetricFieldSet fFieldset; //!< set of fields
  sctime_point fTimestamp;  //!< time stamp
};

} // end namespace cbm

#include "Metric.ipp"

#endif
