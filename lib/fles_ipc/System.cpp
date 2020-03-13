// Copyright 2013-2020 Jan de Cuveland <cmail@cuveland.de>

#include "System.hpp"
#include <cstring>
#include <glob.h>
#include <netdb.h>
#include <pwd.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace fles {
namespace system {

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

  passwd pwd;
  passwd* result;

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

std::vector<std::string> glob(const std::string& pattern) {
  glob_t glob_result;
  memset(&glob_result, 0, sizeof(glob_result));

  int return_value =
      glob(pattern.c_str(), GLOB_BRACE | GLOB_TILDE, NULL, &glob_result);
  if (return_value != 0) {
    globfree(&glob_result);
    throw std::runtime_error("glob() failed with return value " +
                             std::to_string(return_value));
  }

  std::vector<std::string> filenames;
  for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
    filenames.emplace_back(glob_result.gl_pathv[i]);
  }

  globfree(&glob_result);

  return filenames;
}

} // namespace system
} // namespace fles
