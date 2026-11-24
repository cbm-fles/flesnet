// Copyright 2013-2020 Jan de Cuveland <cmail@cuveland.de>

#include "System.hpp"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <glob.h>
// #include <netdb.h>
#include <iomanip>
#include <pwd.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
// #include <sys/socket.h>
// #include <sys/types.h>
// #include <sys/ttycom.h> // definition of winsize and TIOCGWINSZ on MacOS, but
// Linux is missing the file
#include <unistd.h>
#include <vector>

namespace fles::system {

std::string stringerror(int errnum) {
  std::vector<char> buf(256);

#ifndef _GNU_SOURCE

  // XSI-compliant
  int err = strerror_r(errnum, buf.data(), buf.size());
  if (err != 0) {
    return std::string("Unknown error ") + std::to_string(err);
  }

  return {buf.data()};

#else

  // GNU-specific
  char* s = strerror_r(errnum, buf.data(), buf.size());
  return {s};

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
    return {pwd.pw_name};
  }
  return {"unknown"};
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

  return {buf.data()};
}

std::string current_domainname() {
  long bufsize = 1024;

  std::vector<char> buf(static_cast<size_t>(bufsize));

  int err = getdomainname(buf.data(), static_cast<int>(buf.size()));
  if (err != 0) {
    throw std::runtime_error(stringerror(errno));
  }

  return {buf.data()};
}

int current_pid() { return getpid(); }

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

uint16_t current_terminal_width() {
  struct winsize ws{};
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  return ws.ws_col;
}

// Data clock

int64_t DataClock::to_data_time_ns(int64_t real_ns) const {
  if (rate == 1.0) {
    return data_origin_ns + (real_ns - real_origin_ns);
  }
  return data_origin_ns +
         std::llround(static_cast<double>(real_ns - real_origin_ns) * rate);
}

int64_t DataClock::to_real_time_ns(int64_t data_ns) const {
  if (rate == 1.0) {
    return real_origin_ns + (data_ns - data_origin_ns);
  }
  return real_origin_ns +
         std::llround(static_cast<double>(data_ns - data_origin_ns) / rate);
}

std::chrono::nanoseconds DataClock::real_duration(int64_t data_ns) const {
  if (rate == 1.0) {
    return std::chrono::nanoseconds(data_ns);
  }
  return std::chrono::nanoseconds(
      std::llround(static_cast<double>(data_ns) / rate));
}

std::string DataClock::to_string() const {
  if (is_system_clock()) {
    return "system clock";
  }
  std::ostringstream oss;
  oss << format_time_point_ns(data_origin_ns) << " at "
      << format_time_point_ns(real_origin_ns) << ", rate " << rate;
  return oss.str();
}

namespace {
// Set once at startup, before the threads that read the data time start
DataClock g_data_clock;
} // namespace

void set_data_clock(const DataClock& clock) { g_data_clock = clock; }

const DataClock& data_clock() { return g_data_clock; }

uint64_t current_time_ns() {
  if (g_data_clock.is_system_clock()) {
    return current_real_time_ns();
  }
  int64_t data_ns = g_data_clock.to_data_time_ns(
      static_cast<int64_t>(current_real_time_ns()));
  return static_cast<uint64_t>(std::max<int64_t>(data_ns, 0));
}

int64_t parse_time_point_ns(const std::string& str) {
  const auto invalid = [&str] {
    return std::invalid_argument("invalid point in time: \"" + str + "\"");
  };
  if (str.empty()) {
    throw invalid();
  }

  // An integer is a number of nanoseconds since the Unix epoch
  const bool is_integer =
      std::all_of(str.begin() + (str[0] == '-' ? 1 : 0), str.end(),
                  [](unsigned char c) { return std::isdigit(c) != 0; }) &&
      str != "-";
  if (is_integer) {
    try {
      return std::stoll(str);
    } catch (std::exception const&) {
      throw invalid();
    }
  }

  // ISO 8601: date, optional time with fraction, optional zone
  std::tm tm{};
  const char* p = strptime(str.c_str(), "%Y-%m-%d", &tm);
  if (p == nullptr) {
    throw invalid();
  }
  if (*p == 'T' || *p == ' ') {
    p = strptime(p + 1, "%H:%M:%S", &tm);
    if (p == nullptr) {
      throw invalid();
    }
  }
  int64_t fraction_ns = 0;
  if (*p == '.' || *p == ',') {
    ++p;
    int digits = 0;
    while (std::isdigit(static_cast<unsigned char>(*p)) != 0) {
      if (digits < 9) {
        fraction_ns = fraction_ns * 10 + (*p - '0');
        ++digits;
      }
      ++p;
    }
    if (digits == 0) {
      throw invalid();
    }
    for (; digits < 9; ++digits) {
      fraction_ns *= 10;
    }
  }
  std::time_t seconds = 0;
  if (*p == 'Z') {
    seconds = timegm(&tm);
    ++p;
  } else if (*p == '+' || *p == '-') {
    const int sign = (*p == '+') ? 1 : -1;
    ++p;
    char* end = nullptr;
    const long hours = std::strtol(p, &end, 10);
    if (end - p != 2) {
      throw invalid();
    }
    p = end;
    long minutes = 0;
    if (*p == ':') {
      ++p;
      minutes = std::strtol(p, &end, 10);
      if (end - p != 2) {
        throw invalid();
      }
      p = end;
    }
    seconds = timegm(&tm) - sign * (hours * 3600 + minutes * 60);
  } else {
    tm.tm_isdst = -1; // let mktime determine daylight saving time
    seconds = mktime(&tm);
  }
  if (*p != '\0') {
    throw invalid();
  }
  return static_cast<int64_t>(seconds) * 1000000000LL + fraction_ns;
}

std::string format_time_point_ns(int64_t time_ns) {
  // floor division, so that times before the epoch format correctly
  int64_t seconds = time_ns / 1000000000LL;
  int64_t fraction = time_ns % 1000000000LL;
  if (fraction < 0) {
    fraction += 1000000000LL;
    --seconds;
  }
  std::time_t t = seconds;
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
  std::ostringstream oss;
  oss << buf << '.' << std::setw(9) << std::setfill('0') << fraction << 'Z';
  return oss.str();
}

} // namespace fles::system
