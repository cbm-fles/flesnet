// Copyright 2013-2025 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines utility functions in the fles::system namespace.
#pragma once

#include <chrono>
#include <cstdint>
#include <glob.h>
#include <string>
#include <vector>

/// Namespace for system-related utilty functions.
namespace fles::system {
/**
 * \brief Textual description of system error messages.
 *
 * This is a thin C++ wrapper around strerror_r(), which looks up the error
 * message string corresponding to an error number.
 *
 * @param errnum Error number
 * @return message string
 */
std::string stringerror(int errnum);

/**
 * \brief Retrieve current user name.
 *
 * This is a thin C++ wrapper around getpwuid_r(), which obtains information
 * from the system password database.
 *
 * @return user name corresponding to current process
 */
std::string current_username();

/**
 * \brief Retrieve current host name.
 *
 * This is a thin C++ wrapper around gethostname(), which gets the name of the
 * current host.
 *
 * @return current host name
 */
std::string current_hostname();

/**
 * \brief Retrieve current domain name.
 *
 * This is a thin C++ wrapper around getdomainname(), which gets the standard
 * NIS domain name for the current host.
 *
 * @return current domain name
 */
std::string current_domainname();

/**
 * \brief Retrieve current process ID (PID).
 *
 * This is a thin C++ wrapper around getpid(), which returns the process ID of
 * the calling process.
 *
 * @return current process ID
 */
int current_pid();

enum class glob_flags : int {
  none = 0,
  // Always available according to POSIX
  err = GLOB_ERR,
  mark = GLOB_MARK,
  nosort = GLOB_NOSORT,
  nocheck = GLOB_NOCHECK,
  noescape = GLOB_NOESCAPE,
  // Non-standard, but available on both GNU and Darwin
  brace = GLOB_BRACE,
  nomagic = GLOB_NOMAGIC,
  tilde = GLOB_TILDE
};

inline glob_flags operator|(glob_flags a, glob_flags b) {
  return static_cast<glob_flags>(static_cast<int>(a) | static_cast<int>(b));
}

inline glob_flags operator&(glob_flags a, glob_flags b) {
  return static_cast<glob_flags>(static_cast<int>(a) & static_cast<int>(b));
}

inline glob_flags& operator|=(glob_flags& a, glob_flags b) { return a = a | b; }

/**
 * \brief Find pathnames matching a pattern.
 *
 * This is a C++ wrapper around glob(), which searches for all the
 * pathnames matching pattern according to the rules used by the shell.
 *
 * @param pattern pattern for pathnames
 * @param flags flags
 * @return vector of pathnames
 */
std::vector<std::string> glob(const std::string& pattern,
                              glob_flags flags = glob_flags::brace |
                                                 glob_flags::tilde);

/**
 * @brief Get the current terminal width
 *
 * @return terminal width in characters
 */
uint16_t current_terminal_width();

/**
 * \brief The clock that timestamps the data.
 *
 * Timeslice building is keyed on the start times of microslices and
 * timeslices. These are read from the data clock, which by default is the
 * system clock. To replay recorded or simulated data with its original
 * timestamps, the data clock can be offset from the system clock and run at a
 * different rate:
 *
 *     data_time = data_origin + (system_time - real_origin) * rate
 *
 * Every process of a timeslice building setup has to use the same data clock.
 * The data time never goes below zero: before the data clock reaches zero, it
 * reads zero and the chain idles.
 */
struct DataClock {
  int64_t data_origin_ns = 0; ///< data time at the reference instant
  int64_t real_origin_ns = 0; ///< the reference instant in system time
  double rate = 1.0;          ///< data seconds per real second

  /// Whether the data clock is the system clock.
  [[nodiscard]] bool is_system_clock() const {
    return data_origin_ns == real_origin_ns && rate == 1.0;
  }
  /// The data time at a given system time.
  [[nodiscard]] int64_t to_data_time_ns(int64_t real_ns) const;
  /// The system time at which the data clock reads a given data time.
  [[nodiscard]] int64_t to_real_time_ns(int64_t data_ns) const;
  /// The system time that passes while the data clock advances by `data_ns`.
  [[nodiscard]] std::chrono::nanoseconds real_duration(int64_t data_ns) const;
  /// Human-readable description of the clock.
  [[nodiscard]] std::string to_string() const;
};

/**
 * \brief Set the data clock of this process. Call it once at startup, before
 * any thread reads the data time.
 */
void set_data_clock(const DataClock& clock);

/// The data clock of this process.
const DataClock& data_clock();

/**
 * \brief Get the current system time in nanoseconds since the epoch
 *
 * @return current system time in nanoseconds
 */
inline uint64_t current_real_time_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

/**
 * \brief Get the current data time in nanoseconds, see DataClock
 *
 * @return current data time in nanoseconds
 */
uint64_t current_time_ns();

/**
 * \brief Parse a point in time.
 *
 * Accepted are an integer number of nanoseconds since the Unix epoch, and ISO
 * 8601 date-times like `2026-11-16T12:00:00`, optionally with a fraction of a
 * second and a zone designator (`Z` or `+01:00`). Without a zone, the local
 * time zone applies.
 *
 * @return nanoseconds since the Unix epoch
 */
int64_t parse_time_point_ns(const std::string& str);

/**
 * \brief Format a point in time as ISO 8601 in UTC with nanoseconds.
 */
std::string format_time_point_ns(int64_t time_ns);

} // namespace fles::system
