// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

#ifndef included_Cbm_Metric
#define included_Cbm_Metric 1

#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cbm {
using MetricTagSet = std::vector<std::pair<std::string, std::string>>;
using MetricField = std::variant<bool,
                                 int32_t,
                                 uint32_t,
                                 int64_t,
                                 uint64_t,
                                 float,
                                 double,
                                 std::string,
                                 std::string_view>;
using MetricFieldSet = std::vector<std::pair<std::string, MetricField>>;

struct Metric {
  using time_point = std::chrono::system_clock::time_point;

  Metric() = default;
  Metric(const Metric&) = default;
  Metric(Metric&&) = default;
  Metric(const std::string& measurement,
         const MetricTagSet& tagset,
         const MetricFieldSet& fieldset,
         time_point timestamp = time_point());
  Metric(const std::string& measurement,
         const MetricTagSet& tagset,
         MetricFieldSet&& fieldset,
         time_point timestamp = time_point());
  Metric(const std::string& measurement,
         MetricTagSet&& tagset,
         MetricFieldSet&& fieldset,
         time_point timestamp = time_point());
  Metric& operator=(const Metric&) = default;
  Metric& operator=(Metric&&) = default;

  std::string fMeasurement; //!< measurement name
  MetricTagSet fTagset;     //!< set of tags
  MetricFieldSet fFieldset; //!< set of fields
  time_point fTimestamp;    //!< time stamp
};

} // end namespace cbm

#include "Metric.ipp"

#endif
