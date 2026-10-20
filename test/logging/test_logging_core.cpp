// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_logging_core
#include <boost/test/unit_test.hpp>

#include "log.hpp"
#include "log_program_options.hpp"
#include "log_short.hpp"

#include <boost/program_options/errors.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

// stderr is redirected to this file during the tests
char const* const text_file = "test_logging_core.log";
char const* const json_file = "test_logging_core.json";

std::vector<std::string> read_lines(std::string const& filename) {
  logging::flush();
  std::ifstream in(filename);
  std::vector<std::string> lines;
  for (std::string line; std::getline(in, line);) {
    lines.push_back(line);
  }
  return lines;
}

// Lines that contain the given text
std::vector<std::string> grep(std::string const& filename,
                              std::string const& text) {
  std::vector<std::string> result;
  for (auto const& line : read_lines(filename)) {
    if (line.find(text) != std::string::npos) {
      result.push_back(line);
    }
  }
  return result;
}

bool contains(std::string const& line, std::string const& text) {
  return line.find(text) != std::string::npos;
}

// The value of a numeric member of a JSON line, e.g. of "pid"
std::string number(std::string const& line, std::string const& key) {
  auto const begin = line.find("\"" + key + "\":");
  if (begin == std::string::npos) {
    return {};
  }
  auto const first = begin + key.size() + 3;
  return line.substr(first, line.find_first_of(",}", first) - first);
}

logging::Options test_options() {
  logging::Options options;
  options.level = status;
  options.file = json_file;
  options.file_level = debug;
  options.context = {logging::field("run", 1234)};
  return options;
}

// Outputs are process-wide, so set them up once
struct Outputs {
  Outputs() {
    std::fflush(stderr);
    saved_stderr_ = dup(STDERR_FILENO);
    int const fd = open(text_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(fd, STDERR_FILENO);
    close(fd);

    std::remove(json_file); // the log is appended to it
    logging::setup(test_options());
  }

  ~Outputs() {
    logging::flush();
    std::fflush(stderr);
    dup2(saved_stderr_, STDERR_FILENO);
    close(saved_stderr_);
  }

  Outputs(Outputs const&) = delete;
  Outputs& operator=(Outputs const&) = delete;

private:
  int saved_stderr_;
};

// Lets the JSON log file grow by at most the given number of bytes, for the
// lifetime of the object
class FileSizeLimit {
public:
  explicit FileSizeLimit(off_t extra) {
    logging::flush();
    struct stat file_status{};
    BOOST_REQUIRE(stat(json_file, &file_status) == 0);
    BOOST_REQUIRE(getrlimit(RLIMIT_FSIZE, &saved_) == 0);
    // Writing beyond the limit comes with a signal that would end the test:
    // a POSIX one from signal.h, which <csignal> is not known to provide.
    // NOLINTNEXTLINE(misc-include-cleaner)
    std::signal(SIGXFSZ, SIG_IGN);
    rlimit limit = saved_;
    limit.rlim_cur = static_cast<rlim_t>(file_status.st_size + extra);
    BOOST_REQUIRE(setrlimit(RLIMIT_FSIZE, &limit) == 0);
  }
  ~FileSizeLimit() {
    logging::flush();
    setrlimit(RLIMIT_FSIZE, &saved_);
    std::signal(SIGXFSZ, SIG_DFL); // NOLINT(misc-include-cleaner)
  }
  FileSizeLimit(FileSizeLimit const&) = delete;
  FileSizeLimit& operator=(FileSizeLimit const&) = delete;

private:
  rlimit saved_{};
};

int evaluated(int& counter) { return ++counter; }

struct Unformattable {};

// POSIX functions from stdlib.h, which <cstdlib> is not known to provide
void set_env(char const* name, char const* value) {
  setenv(name, value, 1); // NOLINT(misc-include-cleaner)
}
void unset_env(char const* name) {
  unsetenv(name); // NOLINT(misc-include-cleaner)
}

} // namespace

// The formatting library in use comes with log.hpp
// NOLINTBEGIN(misc-include-cleaner)
template <>
struct FLES_LOGGING_FORMAT_NS::formatter<Unformattable>
    : FLES_LOGGING_FORMAT_NS::formatter<std::string_view> {
  auto format(Unformattable const& /* value */,
              FLES_LOGGING_FORMAT_NS::format_context& ctx) const
      -> decltype(ctx.out()) {
    throw std::runtime_error("cannot be formatted");
  }
};
// NOLINTEND(misc-include-cleaner)

BOOST_TEST_GLOBAL_FIXTURE(Outputs);

BOOST_AUTO_TEST_CASE(text_format) {
  L_(info) << "text format " << 42;
  auto const lines = grep(text_file, "text format 42");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  // [HH:MM:SS] INFO: text format 42
  BOOST_CHECK_EQUAL(lines[0].size(), 31);
  BOOST_CHECK_EQUAL(lines[0].substr(9), "] INFO: text format 42");
}

BOOST_AUTO_TEST_CASE(level_filter) {
  BOOST_CHECK(!logging::enabled(trace));
  BOOST_CHECK(logging::enabled(debug));
  int counter = 0;
  L_(trace) << "filtered " << evaluated(counter);
  L_(debug) << "debug only in json " << evaluated(counter);
  BOOST_CHECK_EQUAL(counter, 1);
  BOOST_CHECK(grep(text_file, "filtered").empty());
  BOOST_CHECK(grep(text_file, "debug only in json").empty());
  BOOST_CHECK_EQUAL(grep(json_file, "debug only in json").size(), 1);
}

BOOST_AUTO_TEST_CASE(setup_callback) {
  // Callbacks stay registered, so the state must outlive the test
  static int calls = 0;
  static bool debug_enabled = false;
  logging::on_setup([] {
    ++calls;
    debug_enabled = logging::enabled(debug);
  });
  BOOST_CHECK_EQUAL(calls, 1);
  BOOST_CHECK(debug_enabled); // the JSON file takes debug

  logging::Options options = test_options();
  options.file_level = status;
  logging::setup(options);
  BOOST_CHECK_EQUAL(calls, 2);
  BOOST_CHECK(!debug_enabled);

  logging::setup(test_options());
  BOOST_CHECK_EQUAL(calls, 3);
  BOOST_CHECK(debug_enabled);
}

BOOST_AUTO_TEST_CASE(ostream_log) {
  logging::OstreamLog log(status);
  log.stream << "ostream first part, ";
  log.stream << "second part" << std::endl;
  log.stream << "ostream line one\nline two" << std::endl;
  log.stream << "ostream unflushed";
  BOOST_CHECK_EQUAL(
      grep(text_file, "STATUS: ostream first part, second part").size(), 1);
  BOOST_CHECK_EQUAL(grep(text_file, "STATUS: ostream line one").size(), 1);
  BOOST_CHECK(grep(text_file, "ostream unflushed").empty());
}

BOOST_AUTO_TEST_CASE(ostream_log_destruction) {
  {
    logging::OstreamLog log(status);
    log.stream << "ostream flushed on destruction";
  }
  BOOST_CHECK_EQUAL(grep(text_file, "ostream flushed on destruction").size(),
                    1);
}

BOOST_AUTO_TEST_CASE(json_legacy_message) {
  L_(warning) << "legacy \"quoted\" message";
  auto const lines = grep(json_file, "legacy");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  auto const& line = lines[0];
  BOOST_CHECK(contains(line, R"("severity_code":13,"severity_text":"WARN")"));
  BOOST_CHECK(contains(line, R"("message":"legacy \"quoted\" message")"));
  BOOST_CHECK(contains(line, R"("file_name":"test_logging_core.cpp")"));
  BOOST_CHECK(contains(line, R"("function":"test_method")"));
  BOOST_CHECK(contains(line, R"("service":"test_logging_core")"));
  BOOST_CHECK(contains(line, R"("thread":")"));
  BOOST_CHECK(!number(line, "tid").empty());
  BOOST_CHECK(contains(line, R"("line":)"));
  BOOST_CHECK(!contains(line, R"("line":")"));
  BOOST_CHECK(contains(line, R"("pid":)"));
  BOOST_CHECK(!contains(line, R"("pid":")"));
  BOOST_CHECK(!contains(line, R"("data")"));
  BOOST_CHECK(!contains(line, "message_template"));
  BOOST_CHECK(!contains(line, "logger"));
  BOOST_CHECK(contains(line, R"("run":1234)"));
  // {"@timestamp":"2026-09-30T12:15:02.671480Z",
  auto const prefix = std::string(R"({"@timestamp":")");
  BOOST_REQUIRE(line.size() > prefix.size() + 27);
  BOOST_CHECK_EQUAL(line.substr(0, prefix.size()), prefix);
  auto const timestamp = line.substr(prefix.size(), 27);
  BOOST_CHECK_EQUAL(timestamp[10], 'T');
  BOOST_CHECK_EQUAL(timestamp[19], '.');
  BOOST_CHECK_EQUAL(timestamp.substr(26), "Z");
}

BOOST_AUTO_TEST_CASE(thread_ids) {
  L_(info) << "from the main thread";
  std::thread([] { L_(info) << "from another thread"; }).join();
  auto const main_lines = grep(json_file, "from the main thread");
  auto const other_lines = grep(json_file, "from another thread");
  BOOST_REQUIRE_EQUAL(main_lines.size(), 1);
  BOOST_REQUIRE_EQUAL(other_lines.size(), 1);
  auto const main_tid = number(main_lines[0], "tid");
  auto const other_tid = number(other_lines[0], "tid");
  BOOST_CHECK(std::stoull(main_tid) > 0);
  BOOST_CHECK(std::stoull(other_tid) > 0);
  BOOST_CHECK_NE(main_tid, other_tid);
}

BOOST_AUTO_TEST_CASE(thread_names) {
  std::thread([] {
    logging::set_thread_name("named-thread");
    L_(info) << "from a named thread";
  }).join();
  std::thread([] {
    logging::set_thread_name("a-name-that-is-too-long");
    L_(info) << "from a thread with a long name";
  }).join();
  auto const named = grep(json_file, "from a named thread");
  auto const long_named = grep(json_file, "from a thread with a long name");
  BOOST_REQUIRE_EQUAL(named.size(), 1);
  BOOST_REQUIRE_EQUAL(long_named.size(), 1);
  BOOST_CHECK(contains(named[0], R"("thread":"named-thread")"));
  BOOST_CHECK(contains(long_named[0], R"("thread":"a-name-that-is-")"));
}

BOOST_AUTO_TEST_CASE(thread_renamed) {
  std::thread([] {
    L_(info) << "before the renaming";
    logging::set_thread_name("renamed-thread");
    L_(info) << "after the renaming";
  }).join();
  auto const lines = grep(json_file, R"("message":"after the renaming")");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0], R"("thread":"renamed-thread")"));
}

BOOST_AUTO_TEST_CASE(json_fields) {
  int const n_hits = 42;
  std::string const detector = "sts";
  LF(info, "{} hits in {}", n_hits, detector, logging::field("n_hits", n_hits),
     logging::field("detector", detector), logging::field("fraction", 0.25),
     logging::field("complete", true));
  auto const lines = grep(json_file, "hits in");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  auto const& line = lines[0];
  BOOST_CHECK(contains(line, R"("severity_code":11,"severity_text":"NOTICE")"));
  BOOST_CHECK(contains(line, R"("message":"42 hits in sts")"));
  BOOST_CHECK(contains(line, R"("message_template":"{} hits in {}")"));
  BOOST_CHECK(contains(
      line,
      R"("data":{"n_hits":42,"detector":"sts","fraction":0.25,"complete":true})"));
  BOOST_CHECK_EQUAL(grep(text_file, "INFO: 42 hits in sts").size(), 1);
}

BOOST_AUTO_TEST_CASE(field_as_placeholder_argument) {
  LF(info, "field value {}", logging::field("answer", 42));
  auto const lines = grep(json_file, "field value");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0], R"("message":"field value 42")"));
  BOOST_CHECK(contains(lines[0], R"("data":{"answer":42})"));
}

BOOST_AUTO_TEST_CASE(field_types) {
  enum : std::uint8_t { small = 7 };
  LF(info, "field types", logging::field("char", 'x'),
     logging::field("uint8", static_cast<std::uint8_t>(small)),
     logging::field("literal", "text"), logging::field("negative", -1),
     logging::field("whole", 3.0), logging::field("large", 1e300),
     logging::field("null", static_cast<char const*>(nullptr)));
  auto const lines = grep(json_file, "field types");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0], R"("data":{"char":"x","uint8":7,)"
                                 R"("literal":"text","negative":-1,)"
                                 R"("whole":3.0,"large":1e+300,"null":""})"));
}

// The macros work where a level name is also the name of a variable
BOOST_AUTO_TEST_CASE(macros_next_to_variables) {
  int const error = 3;
  std::string const info = "three";
  ERROR("short macro {} {}", error, info);
  TS_INFO(error, "short timeslice macro {}", info);
  LF(error, "format macro {} {}", error, info);
  LF_TS(info, error, "timeslice macro {}", info);
  L_(info) << "stream macro " << error << " " << info;
  BOOST_CHECK_EQUAL(grep(text_file, "ERROR: short macro 3 three").size(), 1);
  BOOST_CHECK_EQUAL(
      grep(text_file, "INFO: ts3|short timeslice macro three").size(), 1);
  BOOST_CHECK_EQUAL(grep(text_file, "ERROR: format macro 3 three").size(), 1);
  BOOST_CHECK_EQUAL(grep(text_file, "INFO: ts3|timeslice macro three").size(),
                    1);
  BOOST_CHECK_EQUAL(grep(text_file, "INFO: stream macro 3 three").size(), 1);
}

// JSON lines are valid UTF-8, whatever the message holds
BOOST_AUTO_TEST_CASE(json_invalid_utf8) {
  // Valid: two, three and four bytes
  L_(info) << "utf-8 kept: \xc2\xb5 \xe2\x82\xac \xf0\x9f\x98\x80";
  // Invalid: Latin-1, an overlong slash, a surrogate, a cut-off sequence
  L_(info) << "utf-8 replaced: caf\xe9 \xc0\xaf \xed\xa0\x80 \xe2\x82";
  auto lines = grep(json_file, "utf-8 kept");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0],
                       "utf-8 kept: \xc2\xb5 \xe2\x82\xac \xf0\x9f\x98\x80\""));
  lines = grep(json_file, "utf-8 replaced");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0], R"(utf-8 replaced: caf\ufffd \ufffd\ufffd )"
                                 R"(\ufffd\ufffd\ufffd \ufffd\ufffd")"));
}

// A message that cannot be formatted is logged as its format string
BOOST_AUTO_TEST_CASE(format_failure) {
  LF_TS(info, 5, "unformattable {}", Unformattable{},
        logging::field("kept", true));
  auto const lines = grep(json_file, "unformattable");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(
      contains(lines[0], R"("timeslice":5,"message":"unformattable {}")"));
  BOOST_CHECK(
      contains(lines[0],
               R"("data":{"kept":true,"format_error":"cannot be formatted"})"));
  BOOST_CHECK(contains(lines[0], R"("file_name":"test_logging_core.cpp")"));
  BOOST_CHECK_EQUAL(grep(text_file, "INFO: ts5|unformattable {}").size(), 1);
}

// For adapters: the source location need not outlive the call
BOOST_AUTO_TEST_CASE(write_with_copied_location) {
  {
    std::string const file = "/some/path/adapter_source.cxx";
    std::string const function = "AdapterFunction";
    logging::Attributes attributes;
    attributes.file = file;
    attributes.line = 12;
    attributes.function = function;
    logging::write(info, "from another logging system", std::move(attributes));
  }
  auto const lines = grep(json_file, "from another logging system");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(
      lines[0],
      R"("file_name":"adapter_source.cxx","line":12,"function":"AdapterFunction")"));
}

BOOST_AUTO_TEST_CASE(json_without_fields) {
  LF(info, "{} positional and {}", 1, "two");
  auto const lines = grep(json_file, "positional and");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0], R"("message":"1 positional and two")"));
  BOOST_CHECK(
      contains(lines[0], R"("message_template":"{} positional and {}")"));
  BOOST_CHECK(!contains(lines[0], R"("data")"));
}

BOOST_AUTO_TEST_CASE(json_bare_placeholder) {
  LF(info, "{}", "only a placeholder");
  auto const lines = grep(json_file, "only a placeholder");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(!contains(lines[0], "message_template"));
}

BOOST_AUTO_TEST_CASE(timeslice) {
  std::uint64_t const id = 17900000123;
  LF_TS(info, id, "released after {} ms", 37);
  LF_TS(info, 42, "holds {} items", 5, logging::field("n_items", 5));
  LF_TS(info, id, "without arguments");

  BOOST_CHECK_EQUAL(
      grep(text_file, "INFO: ts..123|released after 37 ms").size(), 1);
  BOOST_CHECK_EQUAL(grep(text_file, "INFO: ts42|holds 5 items").size(), 1);
  BOOST_CHECK_EQUAL(grep(text_file, "INFO: ts..123|without arguments").size(),
                    1);

  auto lines = grep(json_file, R"("message":"released after 37 ms")");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0],
                       R"("severity_text":"NOTICE","timeslice":17900000123,)"));
  BOOST_CHECK(
      contains(lines[0], R"("message_template":"released after {} ms")"));
  BOOST_CHECK(!contains(lines[0], R"("data")"));
  BOOST_CHECK(contains(lines[0], R"("function":"test_method")"));

  lines = grep(json_file, R"("message":"holds 5 items")");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0], R"("timeslice":42,)"));
  BOOST_CHECK(contains(lines[0], R"("message_template":"holds {} items")"));
  BOOST_CHECK(contains(lines[0], R"("data":{"n_items":5})"));

  lines = grep(json_file, R"("message":"without arguments")");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0], R"("timeslice":17900000123,)"));
  BOOST_CHECK(contains(lines[0], R"("message_template":"without arguments")"));
}

BOOST_AUTO_TEST_CASE(timeslice_level_filter) {
  int counter = 0;
  LF_TS(trace, evaluated(counter), "filtered timeslice {}", evaluated(counter));
  BOOST_CHECK_EQUAL(counter, 0);
  LF_TS(debug, evaluated(counter), "timeslice in json only");
  BOOST_CHECK_EQUAL(counter, 1);
  BOOST_CHECK(grep(text_file, "timeslice in json only").empty());
  BOOST_CHECK_EQUAL(
      grep(json_file, R"("timeslice":1,"message":"timeslice in json only")")
          .size(),
      1);
}

// For adapters: a message with attributes, but without a format string
BOOST_AUTO_TEST_CASE(write_with_attributes) {
  logging::Attributes attributes;
  attributes.timeslice = 7;
  attributes.fields.push_back(logging::field("source", "adapter"));
  logging::write(info, "written directly", std::move(attributes));
  BOOST_CHECK_EQUAL(grep(text_file, "INFO: ts7|written directly").size(), 1);
  auto const lines = grep(json_file, "written directly");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(
      contains(lines[0], R"("timeslice":7,"message":"written directly")"));
  BOOST_CHECK(contains(lines[0], R"("data":{"source":"adapter"})"));
  BOOST_CHECK(!contains(lines[0], "message_template"));
  BOOST_CHECK(!contains(lines[0], "file_name"));
}

BOOST_AUTO_TEST_CASE(formatted_levels) {
  LF(status, "formatted status");
  LF(debug, "formatted debug");
  LF(trace, "formatted trace");
  BOOST_CHECK_EQUAL(grep(text_file, "STATUS: formatted status").size(), 1);
  BOOST_CHECK(grep(text_file, "formatted debug").empty());
  auto const lines = grep(json_file, "formatted debug");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0], R"("severity_text":"DEBUG")"));
  BOOST_CHECK(grep(json_file, "formatted trace").empty());
}

BOOST_AUTO_TEST_CASE(written_before_abort) {
  // The helper logs a fatal message and aborts right away
  char const* const file = "test_logging_fatal.json";
  std::remove(file);
  std::string const command =
      std::string(TEST_LOGGING_FATAL) + " " + file + " 2>/dev/null";
  BOOST_CHECK(std::system(command.c_str()) != 0);
  BOOST_CHECK_EQUAL(grep(file, R"("message":"filler )").size(), 1000);
  BOOST_CHECK_EQUAL(grep(file, R"("message":"about to abort")").size(), 1);
}

BOOST_AUTO_TEST_CASE(unusable_file) {
  logging::Options options = test_options();
  options.file = "/nonexistent/directory/log.json";
  BOOST_CHECK_THROW(logging::setup(options), std::runtime_error);
  // The outputs are as before
  L_(debug) << "after the failed setup";
  BOOST_CHECK_EQUAL(grep(json_file, "after the failed setup").size(), 1);
}

BOOST_AUTO_TEST_CASE(taken_context_key) {
  logging::Options options = test_options();
  options.context.push_back(logging::field("pid", 1));
  BOOST_CHECK_THROW(logging::setup(options), std::invalid_argument);
  // The outputs are as before
  L_(debug) << "after the refused context";
  BOOST_CHECK_EQUAL(grep(json_file, "after the refused context").size(), 1);
}

// A log file that takes no data is reported on stderr, and so is its return
BOOST_AUTO_TEST_CASE(file_write_failure) {
  {
    FileSizeLimit const limit(0);
    L_(debug) << "lost line 1";
    L_(debug) << "lost line 2";
    logging::flush();
    L_(debug) << "lost line 3";
  }
  L_(debug) << "written after the failure";

  std::string const file = json_file;
  BOOST_CHECK_EQUAL(
      grep(text_file, "ERROR: cannot write to log file " + file + ": ").size(),
      1);
  BOOST_CHECK_EQUAL(grep(text_file, "ERROR: writing to log file " + file +
                                        " again, 3 lines lost")
                        .size(),
                    1);
  BOOST_CHECK(grep(json_file, "lost line").empty());
  BOOST_CHECK_EQUAL(grep(json_file, "written after the failure").size(), 1);
}

// A line that the file takes only in part is ended before the next one
BOOST_AUTO_TEST_CASE(file_line_cut) {
  {
    FileSizeLimit const limit(10);
    L_(debug) << "cut line";
  }
  L_(debug) << "written after the cut line";

  BOOST_CHECK_EQUAL(grep(json_file, "written after the cut line").size(), 1);
  for (auto const& line : read_lines(json_file)) {
    if (line.find("written after the cut line") != std::string::npos) {
      BOOST_CHECK_EQUAL(line.substr(0, 15), R"({"@timestamp":")");
    }
  }
  BOOST_CHECK_EQUAL(grep(text_file, "ERROR: writing to log file " +
                                        std::string(json_file) +
                                        " again, 1 lines lost")
                        .size(),
                    1);
}

BOOST_AUTO_TEST_CASE(logged_after_exit) {
  // The helper logs from a static destructor that runs after the writer
  // thread has stopped
  char const* const file = "test_logging_exit.json";
  std::remove(file);
  std::string const command =
      std::string(TEST_LOGGING_EXIT) + " " + file + " 2>/dev/null";
  BOOST_CHECK(std::system(command.c_str()) == 0);
  BOOST_CHECK_EQUAL(grep(file, R"("message":"before exit")").size(), 1);
  auto const lines = grep(file, R"("message":"after the writer stopped")");
  BOOST_REQUIRE_EQUAL(lines.size(), 1);
  BOOST_CHECK(contains(lines[0], R"("severity_text":"WARN")"));
}

BOOST_AUTO_TEST_CASE(parse) {
  BOOST_CHECK_EQUAL(logging::parse_level("debug"), debug);
  BOOST_CHECK_EQUAL(logging::parse_level("WARNING"), warning);
  BOOST_CHECK_EQUAL(logging::parse_level("warn"), warning);
  BOOST_CHECK_EQUAL(logging::parse_level("3"), info);
  BOOST_CHECK_THROW(logging::parse_level("7"), std::invalid_argument);
  BOOST_CHECK_THROW(logging::parse_level("verbose"), std::invalid_argument);
  BOOST_CHECK(logging::parse_format("json") == logging::Format::json);
  BOOST_CHECK_THROW(logging::parse_format("xml"), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(environment) {
  set_env("CBM_LOG_LEVEL", "debug");
  set_env("CBM_LOG_FORMAT", "json");
  set_env("CBM_LOG_FILE", "env.json");
  set_env("CBM_LOG_FILE_LEVEL", "bogus"); // reported and ignored
  auto const options = logging::options_from_environment();
  unset_env("CBM_LOG_LEVEL");
  unset_env("CBM_LOG_FORMAT");
  unset_env("CBM_LOG_FILE");
  unset_env("CBM_LOG_FILE_LEVEL");
  BOOST_CHECK_EQUAL(options.level, debug);
  BOOST_CHECK(options.format == logging::Format::json);
  BOOST_CHECK_EQUAL(options.file, "env.json");
  BOOST_CHECK(!options.file_level.has_value());
}

BOOST_AUTO_TEST_CASE(command_line) {
  namespace po = boost::program_options;
  po::options_description desc;
  logging::add_program_options(desc);
  auto parse = [&desc](std::vector<char const*> args) {
    po::variables_map vm;
    // Provided by parsers.hpp, which include-cleaner does not recognise
    // NOLINTNEXTLINE(misc-include-cleaner)
    po::store(po::parse_command_line(static_cast<int>(args.size()), args.data(),
                                     desc),
              vm);
    return vm;
  };

  // The command line takes precedence over the environment
  set_env("CBM_LOG_LEVEL", "trace");
  set_env("CBM_LOG_FILE", "env.json");
  auto const options = logging::options_from(
      parse({"test", "-l", "warning", "--log-file-level", "1"}));
  unset_env("CBM_LOG_LEVEL");
  unset_env("CBM_LOG_FILE");
  BOOST_CHECK_EQUAL(options.level, warning);
  BOOST_CHECK(options.format == logging::Format::text);
  BOOST_CHECK_EQUAL(options.file, "env.json");
  BOOST_CHECK_EQUAL(options.file_level.value_or(fatal), debug);

  BOOST_CHECK_THROW(
      logging::options_from(parse({"test", "--log-format", "xml"})), po::error);
}
