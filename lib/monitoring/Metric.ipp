// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

namespace cbm {

/*! \struct Metric
  \brief Describes a Metric data point
*/

//-----------------------------------------------------------------------------
/*! \brief Constructor from components
  \param measurement  measurement id
  \param tagset       set of tags
  \param fieldset     set of fields
  \param timestamp    timestamp (defaults to time of `QueueMetric` when omitted)
 */

inline Metric::Metric(const string& measurement,
                      const MetricTagSet& tagset,
                      const MetricFieldSet& fieldset,
                      sctime_point timestamp)
    : fMeasurement(measurement), fTagset(tagset), fFieldset(fieldset),
      fTimestamp(timestamp) {}

//-----------------------------------------------------------------------------
/*! \brief Constructor from components, move fieldset
  \param measurement  measurement id
  \param tagset       set of tags
  \param fieldset     set of fields (will be moved)
  \param timestamp    timestamp (defaults to time of `QueueMetric` when omitted)
 */

inline Metric::Metric(const string& measurement,
                      const MetricTagSet& tagset,
                      MetricFieldSet&& fieldset,
                      sctime_point timestamp)
    : fMeasurement(measurement), fTagset(tagset), fFieldset(move(fieldset)),
      fTimestamp(timestamp) {}

//-----------------------------------------------------------------------------
/*! \brief Constructor from components, move tagset and fieldset
  \param measurement  measurement id
  \param tagset       set of tagss (will be moved)
  \param fieldset     set of fields (will be moved)
  \param timestamp    timestamp (defaults to time of `QueueMetric` when omitted)
 */

inline Metric::Metric(const string& measurement,
                      MetricTagSet&& tagset,
                      MetricFieldSet&& fieldset,
                      sctime_point timestamp)
    : fMeasurement(measurement), fTagset(move(tagset)),
      fFieldset(move(fieldset)), fTimestamp(timestamp) {}

} // end namespace cbm
