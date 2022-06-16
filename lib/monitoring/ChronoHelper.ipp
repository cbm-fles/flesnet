// SPDX-License-Identifier: GPL-3.0-only
// (C) Copyright 2020-2021 GSI Helmholtzzentrum für Schwerionenforschung
// Original author: Walter F.J. Mueller <w.f.j.mueller@gsi.de>

namespace cbm {
using namespace std;

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Returns the current time as sctime_point
*/

inline sctime_point ScNow() { return chrono::system_clock::now(); }

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Returns the current time as ISO 8601 format string

  Simply returns TimePoint2String() for ScNow()
  Useful as timestamp prefix in messages.
*/

inline string TimeStamp() { return TimePoint2String(ScNow()); }

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Converts chrono duration to integer in msec
  \param  dur   chrono duration
  \returns time in msec
*/

inline long ScDuration2Msec(const scduration& dur) {
  auto dur_msec = chrono::duration_cast<chrono::milliseconds>(dur);
  return long(dur_msec.count());
}

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Converts chrono duration to integer in usec
  \param  dur   chrono duration
  \returns time in usec
*/

inline long ScDuration2Usec(const scduration& dur) {
  auto dur_usec = chrono::duration_cast<chrono::microseconds>(dur);
  return long(dur_usec.count());
}

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Converts chrono duration to integer in nsec
  \param  dur   chrono duration
  \returns time in nsec
*/

inline long ScDuration2Nsec(const scduration& dur) {
  auto dur_nsec = chrono::duration_cast<chrono::nanoseconds>(dur);
  return long(dur_nsec.count());
}

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Converts time given in msec as integer to chrono duration
  \param  msec   time in msec
  \returns chrono duration
*/

inline scduration Msec2ScDuration(long msec) {
  chrono::duration<long, std::milli> tdelta(msec);
  return chrono::duration_cast<scduration>(tdelta);
}

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Converts time given in usec as integer to chrono duration
  \param  usec   time in usec
  \returns chrono duration
*/

inline scduration Usec2ScDuration(long usec) {
  chrono::duration<long, std::micro> tdelta(usec);
  return chrono::duration_cast<scduration>(tdelta);
}

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Converts time given in nsec as integer to chrono duration
  \param  nsec   time in nsec
  \returns chrono duration
*/

inline scduration Nsec2ScDuration(long nsec) {
  chrono::duration<long, std::nano> tdelta(nsec);
  return chrono::duration_cast<scduration>(tdelta);
}

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Converts chrono time_point to integer in nsec
  \param  time   chrono time_point
  \returns time in nsec after the epoch
*/

inline long ScTimePoint2Nsec(const sctime_point& time) {
  chrono::duration<long, std::nano> dt = time - sctime_point();
  return dt.count();
}

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Converts current time to integer in nsec
  \returns time in nsec after the epoch
*/

inline long ScTimePoint2Nsec() { return ScTimePoint2Nsec(ScNow()); }

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Converts time given in nsec after the epoch to chrono time point
  \param  nsec   time in nsec
  \returns chrono time point
*/

inline sctime_point Nsec2ScTimePoint(long nsec) {
  chrono::duration<long, std::nano> tdelta(nsec);
  return sctime_point() + chrono::duration_cast<scduration>(tdelta);
}

//-----------------------------------------------------------------------------
/*!
  \ingroup ChronoHelper
  \brief Returns time difference between two time points in sec as double
  \param  tbeg   start time point
  \param  tend   end time point
  \returns time difference in sec
*/

inline double ScTimeDiff2Double(const sctime_point& tbeg,
                                const sctime_point& tend) {
  chrono::duration<double> dt = tend - tbeg;
  return dt.count();
}

} // end namespace cbm
