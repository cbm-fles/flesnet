// Copyright 2013-2022 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines utility functions in the cbm::system namespace.
#pragma once

#include <glob.h>
#include <string>
#include <vector>

namespace cbm {
/// Namespace for system-related utilty functions.
namespace system {
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

/**
 * \brief Retrieve current thread name.
 *
 * This is a thin C++ wrapper around pthread_getname_np(), which returns the
 * thread name of the current thread.
 *
 * @return current thread name
 */
std::string current_thread_name();

/**
 * \brief Set the current thread name.
 *
 * This is a thin C++ wrapper around pthread_setname_np(), which sets the
 * thread name of the current thread.
 *
 * This name has no operational relevance and is only used in tools accessing
 * them via `/proc`, like `top`. This allows for example to identify the threads
 * in a threads view display of `top` (`H` mode).
 *
 * \note The Linux kernel limits the thread name length to 16 characters.
 * `tname` will be truncated if longer.
 *
 * @param tname  thread name
 */
void set_thread_name(const std::string& tname [[maybe_unused]]);

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
 * @return vector of pathnames
 */
std::vector<std::string> glob(const std::string& pattern,
                              glob_flags flags = glob_flags::brace |
                                                 glob_flags::tilde);

} // namespace system
} // namespace cbm
