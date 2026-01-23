// Copyright 2013-2022 Jan de Cuveland <cmail@cuveland.de>

#include "SystemInfo.hpp"
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <pthread.h>
#include <pwd.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace cbm::system {

std::string stringerror(int errnum) {
  std::vector<char> buf(256);

#ifndef _GNU_SOURCE

  // XSI-compliant
  int err = strerror_r(errnum, buf.data(), buf.size());
  if (err != 0) {
    return std::string("Unknown error ") + std::to_string(err);
  }

  return std::string(buf.data());

#else

  // GNU-specific
  char* s = strerror_r(errnum, buf.data(), buf.size());
  return std::string(s);

#endif
}

std::string current_username() {
  uid_t uid = getuid();

  long bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (bufsize == -1) {
    bufsize = 16384;
  }

  std::vector<char> buf(static_cast<size_t>(bufsize));

  passwd pwd{};
  passwd* result = nullptr;

  int err = getpwuid_r(uid, &pwd, buf.data(), buf.size(), &result);

  if (err != 0) {
    throw std::runtime_error(stringerror(err));
  }

  if (result != nullptr) {
    return std::string(pwd.pw_name);
  }
  return std::string("unknown");
}

std::string current_hostname() {
  long bufsize = sysconf(_SC_HOST_NAME_MAX) + 1;
  if (bufsize == -1) {
    bufsize = 257;
  }

  std::vector<char> buf(static_cast<size_t>(bufsize));

  int err = gethostname(buf.data(), buf.size());
  if (err != 0) {
    throw std::runtime_error(stringerror(errno));
  }

  return std::string(buf.data());
}

std::string current_domainname() {
  long bufsize = 1024;

  std::vector<char> buf(static_cast<size_t>(bufsize));

  int err = getdomainname(buf.data(), static_cast<int>(buf.size()));
  if (err != 0) {
    throw std::runtime_error(stringerror(errno));
  }

  return std::string(buf.data());
}

int current_pid() { return getpid(); }

std::string current_thread_name() {
#if defined(HAVE_PTHREAD_SETNAME_NP)
  // Note: the kernel limit is 16 char, see TASK_COMM_LEN
  static constexpr size_t TASK_COMM_LEN = 16;
  char tname[TASK_COMM_LEN + 1] = {0};

  int err = pthread_getname_np(pthread_self(), &tname[0], TASK_COMM_LEN);
  if (err != 0) {
    throw std::runtime_error(stringerror(errno));
  }

  return string(tname);
#else
  return std::string();
#endif
}

void set_thread_name(const std::string& tname [[maybe_unused]]) {
#if defined(HAVE_PTHREAD_SETNAME_NP)
  // Note: the kernel limit is 16 char, see TASK_COMM_LEN; so truncate
  static constexpr size_t TASK_COMM_LEN = 16;
  string tname16 = tname;
  if (tname16.length() > TASK_COMM_LEN)
    tname16.resize(TASK_COMM_LEN);

  int err = pthread_setname_np(pthread_self(), tname16.c_str());
  if (err != 0) {
    throw std::runtime_error(stringerror(errno));
  }
#endif
}

std::vector<std::string> glob(const std::string& pattern, glob_flags flags) {
  glob_t glob_result{};

  int return_value =
      glob(pattern.c_str(), static_cast<int>(flags), nullptr, &glob_result);
  if (return_value != 0) {
    globfree(&glob_result);
    std::string message;
    switch (return_value) {
    case GLOB_NOMATCH:
      message = "no matches found for pattern: \"" + pattern + "\"";
      break;
    case GLOB_ABORTED:
      message = "read error";
      break;
    case GLOB_NOSPACE:
      message = "out of memory";
      break;
    default:
      message = "failed with return value " + std::to_string(return_value);
    }
    throw std::runtime_error("glob(): " + message);
  }

  std::vector<std::string> filenames;
  for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
    filenames.emplace_back(glob_result.gl_pathv[i]);
  }

  globfree(&glob_result);

  return filenames;
}

} // namespace cbm::system
