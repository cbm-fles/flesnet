// Copyright 2014, 2026 Jan de Cuveland <cmail@cuveland.de>
#include "log.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <fcntl.h>
#include <functional>
#include <ios>
#include <memory>
#include <mutex>
#include <ostream>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr std::array<char const*, 7> level_names{
    "TRACE", "DEBUG", "STATUS", "INFO", "WARNING", "ERROR", "FATAL"};

// Out-of-range levels count as fatal
std::size_t level_index(severity_level level) {
  return std::min(static_cast<std::size_t>(level), level_names.size() - 1);
}

// Severity in JSON output: OpenTelemetry numbers and names, as used by the
// DCA logger, so that both can share one log store. status and info become
// INFO and NOTICE.
constexpr std::array<int, 7> severity_codes{1, 5, 9, 11, 13, 17, 21};
constexpr std::array<std::string_view, 7> severity_texts{
    "TRACE", "DEBUG", "INFO", "NOTICE", "WARN", "ERROR", "FATAL"};

// Whole-line colours of text output on a terminal
constexpr std::array<std::string_view, 7> level_colours{
    "",                 // trace
    "\033[36m",         // debug: cyan
    "\033[32m",         // status: green
    "\033[97m\033[1m",  // info: bold white
    "\033[33m\033[1m",  // warning: bold yellow
    "\033[31m\033[1m",  // error: bold red
    "\033[1m\033[41m"}; // fatal: bold on red
constexpr std::string_view colour_reset{"\033[0m"};

// The writer thread lets messages accumulate for this long, so that the
// threads that log rarely have to wake it.
constexpr std::chrono::milliseconds write_interval{10};

// Formatted messages are written once they reach this size, in bytes.
constexpr std::size_t write_chunk_size = std::size_t{64} * 1024;

// What the messages waiting to be written may take, in bytes. Beyond that,
// messages are dropped and counted: logging never waits for the outputs.
constexpr std::size_t max_pending_bytes = std::size_t{100} * 1024 * 1024;

// At exit, the writer thread is given up if it writes nothing for this long,
// so that an output which takes no more data does not keep the process alive.
constexpr std::chrono::seconds exit_timeout{2};

// A fatal message waits at most this long until it is written, so that an
// output which takes no more data does not keep the process from ending.
constexpr std::chrono::seconds fatal_timeout{2};

// The members that every JSON line has, or can have. The context of setup()
// must not repeat them.
constexpr std::array<std::string_view, 15> json_keys{"@timestamp",
                                                     "host",
                                                     "service",
                                                     "thread",
                                                     "tid",
                                                     "severity_code",
                                                     "severity_text",
                                                     "timeslice",
                                                     "message",
                                                     "message_template",
                                                     "data",
                                                     "file_name",
                                                     "line",
                                                     "function",
                                                     "pid"};

char const* process_name() {
#ifdef __APPLE__
  return getprogname();
#else
  // A GNU extension in errno.h, which <cerrno> is not known to provide
  return program_invocation_short_name; // NOLINT(misc-include-cleaner)
#endif
}

// The id that the system has for the calling thread
std::uint64_t thread_id() {
#ifdef __APPLE__
  std::uint64_t id = 0;
  pthread_threadid_np(nullptr, &id);
  return id;
#else
  return static_cast<std::uint64_t>(gettid());
#endif
}

// What identifies a thread in its messages. Threads that were not given a
// name have that of the thread which started them, so the id tells them
// apart.
struct Thread {
  std::string name;
  std::uint64_t id;
};

// The calling thread, with the name it had at its first message or was
// given later by logging::set_thread_name()
Thread& this_thread() {
  thread_local Thread thread = [] {
    std::array<char, 32> name{};
    pthread_getname_np(pthread_self(), name.data(), name.size());
    return Thread{name.data(), thread_id()};
  }();
  return thread;
}

// The last component of a path
std::string_view file_name(std::string_view path) {
  auto const slash = path.rfind('/');
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

// One message on its way to the outputs. It holds copies of what its
// attributes refer to, so that their source, e.g. a shared library, need not
// outlive it.
struct Record {
  Record(severity_level level,
         std::string message,
         logging::Attributes&& attributes,
         std::chrono::system_clock::time_point time = {})
      : time(time), level(level), message(std::move(message)),
        line(attributes.line), timeslice(attributes.timeslice),
        fields(std::move(attributes.fields)), thread(this_thread()) {
    auto const file = file_name(attributes.file);
    strings.reserve(file.size() + attributes.function.size() +
                    attributes.message_template.size());
    strings += file;
    strings += attributes.function;
    strings += attributes.message_template;
    file_size = file.size();
    function_size = attributes.function.size();
  }

  // The last component of the source file name; empty if there is no
  // source location
  [[nodiscard]] std::string_view file() const {
    return std::string_view(strings).substr(0, file_size);
  }
  [[nodiscard]] std::string_view function() const {
    return std::string_view(strings).substr(file_size, function_size);
  }
  [[nodiscard]] std::string_view message_template() const {
    return std::string_view(strings).substr(file_size + function_size);
  }

  std::chrono::system_clock::time_point time;
  severity_level level;
  std::string message;
  std::string strings; ///< the file name, function and message template
  std::size_t file_size = 0;
  std::size_t function_size = 0;
  unsigned line;
  std::optional<std::uint64_t> timeslice;
  std::vector<logging::Field> fields;
  Thread thread;
};

// About what a queued message takes, in bytes
std::size_t memory_size(Record const& record) {
  std::size_t size =
      sizeof(Record) + record.message.size() + record.strings.size();
  for (auto const& field : record.fields) {
    size += sizeof(logging::Field) + field.key.size();
    if (auto const* text = std::get_if<std::string>(&field.value)) {
      size += text->size();
    }
  }
  return size;
}

// Write the text to the file descriptor. Returns how much of it is written:
// if not all, errno tells why.
std::size_t write_all(int fd, std::string_view text) {
  std::size_t written = 0;
  while (written < text.size()) {
    auto const n = ::write(fd, text.data() + written, text.size() - written);
    if (n < 0 && errno == EINTR) {
      continue;
    }
    if (n <= 0) {
      if (n == 0) {
        errno = EIO;
      }
      break;
    }
    written += static_cast<std::size_t>(n);
  }
  return written;
}

// The JSON log file
class File {
public:
  explicit File(std::string const& path)
      : path_(path),
        fd_(open(
            path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0666)) {
    if (fd_ < 0) {
      throw std::runtime_error("cannot open log file " + path + ": " +
                               std::strerror(errno));
    }
  }
  ~File() { close(fd_); }
  File(File const&) = delete;
  File& operator=(File const&) = delete;

  // Append whole lines. They go to the file in one write as a rule, which
  // keeps them whole when several processes append to it; a write that the
  // system takes only in part can split a line then.
  //
  // Returns what there is to report: that writing fails, once, or that it
  // works again.
  std::string write(std::string_view lines) {
    // A line that a failed write left unfinished is ended first, so that it
    // does not run into the next one
    if (unfinished_line_ && write_all(fd_, "\n") == 1) {
      unfinished_line_ = false;
    }
    auto const written = unfinished_line_ ? 0 : write_all(fd_, lines);
    if (written == lines.size()) {
      if (n_lost_ == 0) {
        return {};
      }
      auto const n_lost = std::exchange(n_lost_, 0);
      return "writing to log file " + path_ + " again, " +
             std::to_string(n_lost) + " lines lost";
    }
    std::string const reason = std::strerror(errno);
    bool const first = n_lost_ == 0;
    unfinished_line_ =
        unfinished_line_ || (written > 0 && lines[written - 1] != '\n');
    lines.remove_prefix(written);
    n_lost_ += static_cast<std::uint64_t>(
        std::count(lines.begin(), lines.end(), '\n'));
    return first ? "cannot write to log file " + path_ + ": " + reason
                 : std::string();
  }

private:
  std::string path_;
  int fd_;
  std::uint64_t n_lost_ = 0;     ///< lines not written since writing fails
  bool unfinished_line_ = false; ///< the file ends in part of a line
};

// Where messages go. Replaced as a whole by setup().
struct Outputs {
  severity_level console_level = status;
  logging::Format console_format = logging::Format::text;
  // A person watches: text is coloured, and each message is written by its
  // thread, to keep the order with other terminal output
  bool interactive = false;
  severity_level file_level = status;
  std::unique_ptr<File> file;
  std::vector<logging::Field> context;

  [[nodiscard]] severity_level minimum() const {
    return file ? std::min(console_level, file_level) : console_level;
  }
};

bool stderr_is_a_tty() {
  return (isatty(STDERR_FILENO) != 0) && (getenv("TERM") != nullptr);
}

// Throws std::invalid_argument if a context key is taken, and
// std::runtime_error if the file cannot be opened
Outputs make_outputs(logging::Options const& options) {
  for (auto const& field : options.context) {
    if (std::find(json_keys.begin(), json_keys.end(), field.key) !=
        json_keys.end()) {
      throw std::invalid_argument("log context key is taken: " + field.key);
    }
  }
  Outputs outputs;
  outputs.console_level = options.level;
  outputs.console_format = options.format;
  outputs.interactive =
      options.format == logging::Format::text && stderr_is_a_tty();
  outputs.file_level = options.file_level.value_or(options.level);
  if (!options.file.empty()) {
    outputs.file = std::make_unique<File>(options.file);
  }
  outputs.context = options.context;
  return outputs;
}

template <typename T> void append_number(std::string& out, T value) {
  std::array<char, 32> buffer{};
  auto const result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  out.append(buffer.data(), result.ptr);
}

// The length of the UTF-8 sequence that the text starts with, in bytes; 0 if
// it starts with none. The text is not empty.
std::size_t utf8_length(std::string_view text) {
  auto byte = [text](std::size_t i) {
    return static_cast<unsigned char>(text[i]);
  };
  // Whether the sequence continues up to this byte
  auto continues = [&](std::size_t last) {
    for (std::size_t i = 1; i <= last; ++i) {
      if (i >= text.size() || (byte(i) & 0xC0U) != 0x80U) {
        return false;
      }
    }
    return true;
  };
  auto const first = byte(0);
  if (first < 0x80) {
    return 1;
  }
  if (first >= 0xC2 && first <= 0xDF) {
    return continues(1) ? 2 : 0;
  }
  if (first >= 0xE0 && first <= 0xEF) {
    // Not what two bytes can encode, and no surrogates
    bool const valid = continues(2) && (first != 0xE0 || byte(1) >= 0xA0) &&
                       (first != 0xED || byte(1) <= 0x9F);
    return valid ? 3 : 0;
  }
  if (first >= 0xF0 && first <= 0xF4) {
    // Not what three bytes can encode, and nothing beyond U+10FFFF
    bool const valid = continues(3) && (first != 0xF0 || byte(1) >= 0x90) &&
                       (first != 0xF4 || byte(1) <= 0x8F);
    return valid ? 4 : 0;
  }
  return 0;
}

// A byte that is not part of valid UTF-8 becomes U+FFFD, since JSON must be
// valid UTF-8.
void append_json_string(std::string& out, std::string_view text) {
  out += '"';
  std::size_t copied = 0; // text up to here is in out
  for (std::size_t i = 0; i < text.size(); ++i) {
    char const c = text[i];
    auto const byte = static_cast<unsigned char>(c);
    if (byte >= 0x80) {
      auto const length = utf8_length(text.substr(i));
      if (length > 0) {
        i += length - 1;
        continue;
      }
    } else if (c != '"' && c != '\\' && byte >= 0x20) {
      continue;
    }
    out.append(text, copied, i - copied);
    copied = i + 1;
    if (byte >= 0x80) {
      out += "\\ufffd";
      continue;
    }
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default: {
      std::array<char, 8> buffer{};
      std::snprintf(buffer.data(), buffer.size(), "\\u%04x",
                    static_cast<unsigned>(c));
      out += buffer.data();
    }
    }
  }
  out.append(text, copied, text.size() - copied);
  out += '"';
}

void append_json_member(std::string& out,
                        std::string_view key,
                        std::string_view value) {
  out += ',';
  append_json_string(out, key);
  out += ':';
  append_json_string(out, value);
}

void append_json_value(std::string& out, logging::Field::Value const& value) {
  std::visit(
      [&out](auto const& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
          append_json_string(out, v);
        } else if constexpr (std::is_same_v<T, bool>) {
          out += v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, double>) {
          if (std::isfinite(v)) {
            // A whole number keeps its point, e.g. 3.0, so that a reader of
            // the JSON does not take the field for an integer one
            auto const begin = out.size();
            append_number(out, v);
            if (out.find_first_of(".e", begin) == std::string::npos) {
              out += ".0";
            }
          } else {
            out += "null"; // JSON has no NaN or infinity
          }
        } else {
          append_number(out, v);
        }
      },
      value);
}

void append_json_field(std::string& out, logging::Field const& field) {
  append_json_string(out, field.key);
  out += ':';
  append_json_value(out, field.value);
}

// Formats the messages. Keeps state, so it is for one thread at a time.
class Formatter {
public:
  Formatter() : service_(process_name()), pid_(std::to_string(getpid())) {
    std::array<char, 256> host{};
    gethostname(host.data(), host.size() - 1);
    host_ = host.data();
  }

  // [HH:MM:SS] LEVEL: message, with a timeslice id in front of the message
  // as "ts123|", shortened to "ts..123|" beyond six digits
  void append_text(std::string& out, Outputs const& outputs, Record const& r) {
    auto const level = level_index(r.level);
    if (outputs.interactive) {
      out += level_colours[level];
    }
    out += '[';
    append_local_time(out, r.time);
    out += "] ";
    out += level_names[level];
    out += ": ";
    if (r.timeslice) {
      std::uint64_t const id = *r.timeslice;
      if (id >= 1'000'000) {
        std::array<char, 16> buffer{};
        std::snprintf(buffer.data(), buffer.size(), "ts..%03u|",
                      static_cast<unsigned>(id % 1000));
        out += buffer.data();
      } else {
        out += "ts";
        append_number(out, id);
        out += '|';
      }
    }
    out += r.message;
    if (outputs.interactive) {
      out += colour_reset;
    }
    out += '\n';
  }

  // One JSON object per line with the field names of the DCA logger:
  // @timestamp, host, service, thread, severity_code/_text, message and the
  // fields as data. In addition the thread id as tid, the timeslice id as
  // timeslice, the format string as message_template, the source location,
  // pid and the context from setup().
  void append_json(std::string& out, Outputs const& outputs, Record const& r) {
    auto const level = level_index(r.level);
    out += R"({"@timestamp":")";
    append_utc_timestamp(out, r.time);
    out += '"';
    append_json_member(out, "host", host_);
    append_json_member(out, "service", service_);
    append_json_member(out, "thread", r.thread.name);
    out += R"(,"tid":)";
    append_number(out, r.thread.id);
    out += R"(,"severity_code":)";
    append_number(out, severity_codes[level]);
    append_json_member(out, "severity_text", severity_texts[level]);
    if (r.timeslice) {
      out += R"(,"timeslice":)";
      append_number(out, *r.timeslice);
    }
    append_json_member(out, "message", r.message);
    // A template that is just a placeholder says nothing about the message
    auto const message_template = r.message_template();
    if (!message_template.empty() && message_template != "{}") {
      append_json_member(out, "message_template", message_template);
    }
    if (!r.fields.empty()) {
      out += R"(,"data":{)";
      char const* separator = "";
      for (auto const& field : r.fields) {
        out += separator;
        separator = ",";
        append_json_field(out, field);
      }
      out += '}';
    }
    if (!r.file().empty()) {
      append_json_member(out, "file_name", r.file());
      out += R"(,"line":)";
      append_number(out, r.line);
      append_json_member(out, "function", r.function());
    }
    out += R"(,"pid":)";
    out += pid_;
    for (auto const& field : outputs.context) {
      out += ',';
      append_json_field(out, field);
    }
    out += "}\n";
  }

private:
  using TimePoint = std::chrono::system_clock::time_point;

  void append_local_time(std::string& out, TimePoint time) {
    auto const seconds = std::chrono::system_clock::to_time_t(time);
    if (seconds != local_seconds_) {
      std::tm tm{};
      localtime_r(&seconds, &tm);
      std::array<char, 16> buffer{};
      auto const n =
          std::strftime(buffer.data(), buffer.size(), "%H:%M:%S", &tm);
      local_time_.assign(buffer.data(), n);
      local_seconds_ = seconds;
    }
    out += local_time_;
  }

  // UTC in ISO 8601 with microseconds, e.g. 2026-09-30T12:15:02.671480Z
  void append_utc_timestamp(std::string& out, TimePoint time) {
    auto const seconds = std::chrono::system_clock::to_time_t(time);
    if (seconds != utc_seconds_) {
      std::tm tm{};
      gmtime_r(&seconds, &tm);
      std::array<char, 32> buffer{};
      auto const n =
          std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%S", &tm);
      utc_date_time_.assign(buffer.data(), n);
      utc_seconds_ = seconds;
    }
    auto const micro =
        std::chrono::duration_cast<std::chrono::microseconds>(
            time - std::chrono::system_clock::from_time_t(seconds))
            .count();
    std::array<char, 16> buffer{};
    auto const n = std::snprintf(buffer.data(), buffer.size(), ".%06uZ",
                                 static_cast<unsigned>(micro));
    out += utc_date_time_;
    out.append(buffer.data(), static_cast<std::size_t>(n));
  }

  // The fields that identify the process in every JSON line
  std::string host_;
  std::string service_;
  std::string pid_;

  std::time_t local_seconds_ = -1;
  std::string local_time_;
  std::time_t utc_seconds_ = -1;
  std::string utc_date_time_;
};

// Minimum level over all outputs, read on every log call. Negative until
// the core exists.
std::atomic<int> g_minimum{-1};

class Core;
Core& core();
void on_exit();

// The queue of messages and the thread that writes them. Any thread can
// also write the queued messages itself, see drain().
class Core {
public:
  Core() {
    auto options = logging::options_from_environment();
    try {
      outputs_ = make_outputs(options);
    } catch (std::runtime_error const& e) {
      std::fprintf(stderr, "fles_logging: %s\n", e.what());
      options.file.clear();
      outputs_ = make_outputs(options);
    }
    apply_outputs();
    // The writer thread leaves all signals to the other threads
    sigset_t all;
    sigset_t previous;
    sigfillset(&all);
    pthread_sigmask(SIG_SETMASK, &all, &previous);
    writer_ = std::thread([this] { run(); });
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    std::atexit(on_exit);
  }

  // Queue the message. This does not wait for the outputs, except where the
  // message is to be written before the call returns. A fatal message waits
  // for the writer thread, for a limited time.
  void submit(Record&& record) {
    auto const level = record.level;
    auto const size = memory_size(record);
    bool write_now = false;
    bool wake_writer = false;
    std::uint64_t n_queued = 0;
    {
      std::scoped_lock const lock(queue_mutex_);
      if (pending_bytes_ + size > max_pending_bytes && level < fatal) {
        ++n_dropped_;
        return;
      }
      // Taken here, so that the messages are queued in the order of time
      record.time = std::chrono::system_clock::now();
      // An error is not left waiting, so that little of it is lost if the
      // process ends abnormally
      urgent_ = urgent_ || level >= error;
      wake_writer = queue_.empty() || level >= error;
      queue_.push_back(std::move(record));
      pending_bytes_ += size;
      n_queued = ++n_queued_;
      write_now = synchronous_ || writer_ended_;
    }
    if (write_now) {
      drain();
      return;
    }
    if (wake_writer) {
      wake_.notify_one();
    }
    if (level >= fatal) {
      std::unique_lock lock(queue_mutex_);
      written_.wait_for(lock, fatal_timeout,
                        [&] { return n_written_ >= n_queued; });
    }
  }

  // Write the queued messages on the calling thread. On return, all
  // messages queued before the call are written.
  void drain() {
    std::scoped_lock const lock(output_mutex_);
    drain_locked();
  }

  void setup(logging::Options const& options) {
    Outputs outputs = make_outputs(options);
    {
      std::scoped_lock const lock(output_mutex_);
      // Messages logged so far go to the previous outputs
      drain_locked();
      outputs_ = std::move(outputs);
      apply_outputs();
    }
    std::scoped_lock const lock(callback_mutex_);
    for (auto const& callback : setup_callbacks_) {
      callback();
    }
  }

  void on_setup(std::function<void()> callback) {
    std::scoped_lock const lock(callback_mutex_);
    callback();
    setup_callbacks_.push_back(std::move(callback));
  }

  // Let the writer thread write the queued messages and end. Later messages
  // are written by the thread that logs them. If the writer thread is stuck
  // in an output, it is left alone, and later messages are lost.
  void stop() {
    // Not in a child process, which has no writer thread
    if (getpid() != pid_) {
      return;
    }
    {
      std::unique_lock lock(queue_mutex_);
      if (stop_) {
        return;
      }
      stop_ = true;
      wake_.notify_one();
      // Wait for the writer thread as long as it gets on
      auto n_writes = n_writes_.load();
      while (!ended_.wait_for(lock, exit_timeout,
                              [this] { return writer_ended_; })) {
        auto const n = n_writes_.load();
        if (n == n_writes) {
          return;
        }
        n_writes = n;
      }
    }
    writer_.join();
  }

private:
  // Call with the output mutex held (or from the constructor)
  void apply_outputs() {
    {
      std::scoped_lock const lock(queue_mutex_);
      synchronous_ = outputs_.interactive;
    }
    g_minimum.store(outputs_.minimum(), std::memory_order_relaxed);
  }

  // Call with the output mutex held
  void drain_locked() {
    std::size_t batch_bytes = 0;
    std::uint64_t n_dropped = 0;
    std::uint64_t n_queued = 0;
    {
      std::scoped_lock const lock(queue_mutex_);
      batch_.swap(queue_);
      batch_bytes = pending_bytes_; // no other batch is under way
      n_dropped = std::exchange(n_dropped_, 0);
      n_queued = n_queued_;
    }
    try {
      for (auto const& record : batch_) {
        append(record);
      }
      if (n_dropped > 0) {
        logging::Attributes attributes;
        attributes.fields.push_back(logging::field("n_dropped", n_dropped));
        append(Record(error,
                      std::to_string(n_dropped) +
                          " log messages dropped, too many were waiting",
                      std::move(attributes), std::chrono::system_clock::now()));
      }
    } catch (...) { // NOLINT(bugprone-empty-catch): write what there is
    }
    write_out();
    batch_.clear();
    {
      std::scoped_lock const lock(queue_mutex_);
      pending_bytes_ -= batch_bytes;
      n_written_ = n_queued;
    }
    written_.notify_all();
  }

  void append_to_console(Record const& record) {
    if (record.level >= outputs_.console_level) {
      if (outputs_.console_format == logging::Format::json) {
        formatter_.append_json(console_, outputs_, record);
      } else {
        formatter_.append_text(console_, outputs_, record);
      }
    }
  }

  void append(Record const& record) {
    append_to_console(record);
    if (outputs_.file && record.level >= outputs_.file_level) {
      formatter_.append_json(file_, outputs_, record);
    }
    if (console_.size() + file_.size() >= write_chunk_size) {
      write_out();
    }
  }

  void write_out() {
    if (!file_.empty()) {
      // The file first: what it has to report goes to stderr
      try {
        auto report = outputs_.file->write(file_);
        if (!report.empty()) {
          append_to_console(Record(error, std::move(report), {},
                                   std::chrono::system_clock::now()));
        }
      } catch (...) { // NOLINT(bugprone-empty-catch): nowhere to report to
      }
      file_.clear();
    }
    if (!console_.empty()) {
      // Not via stdio: a thread that is stuck here would hold the lock of
      // stderr, which the process needs to end
      write_all(STDERR_FILENO, console_);
      console_.clear();
    }
    ++n_writes_;
  }

  void run() {
    logging::set_thread_name("fles_logging");
    std::unique_lock lock(queue_mutex_);
    for (;;) {
      wake_.wait(lock, [this] { return !queue_.empty() || stop_; });
      if (stop_ && queue_.empty() && n_dropped_ == 0) {
        break;
      }
      // Let messages accumulate, unless one of them is urgent
      wake_.wait_for(lock, write_interval, [this] { return urgent_ || stop_; });
      urgent_ = false;
      lock.unlock();
      drain();
      lock.lock();
    }
    // The queue is empty, and what enters it from now on is written by the
    // thread that logs it
    writer_ended_ = true;
    ended_.notify_all();
  }

  std::mutex queue_mutex_;
  std::condition_variable wake_;    ///< for the writer thread
  std::condition_variable ended_;   ///< the writer thread has ended
  std::condition_variable written_; ///< n_written_ has increased
  std::vector<Record> queue_;
  std::size_t pending_bytes_ = 0; ///< of the queue and the batch
  std::uint64_t n_queued_ = 0;    ///< messages queued so far
  std::uint64_t n_written_ = 0;   ///< of them, the ones written
  std::uint64_t n_dropped_ = 0;   ///< messages not queued, to be reported
  bool urgent_ = false;           ///< the queue is to be written at once
  bool synchronous_ = false;      ///< each message is written by its thread
  bool stop_ = false;             ///< the writer thread is to end
  bool writer_ended_ = false;     ///< and it has

  std::mutex output_mutex_; ///< taken before queue_mutex_, never after it
  Outputs outputs_;
  Formatter formatter_;
  std::vector<Record> batch_; ///< the messages being written
  std::string console_;
  std::string file_;
  std::atomic<std::uint64_t> n_writes_{0}; ///< shows that the outputs take data

  std::mutex callback_mutex_;
  std::vector<std::function<void()>> setup_callbacks_;

  // Provided by unistd.h, which include-cleaner does not recognise
  pid_t const pid_ = getpid(); // NOLINT(misc-include-cleaner)
  std::thread writer_;
};

// Never destroyed, so that logging works during static destruction
Core& core() {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  static auto* const instance = new Core();
  return *instance;
}

void on_exit() {
  try {
    core().stop();
  } catch (...) { // NOLINT(bugprone-empty-catch): nothing left to report to
  }
}

} // namespace

std::ostream& operator<<(std::ostream& strm, severity_level level) {
  auto const index = static_cast<std::size_t>(level);
  if (index < level_names.size()) {
    strm << level_names[index];
  } else {
    strm << static_cast<int>(level);
  }
  return strm;
}

namespace logging {

void set_thread_name(std::string_view name) noexcept {
  // Longer names are refused on Linux
  std::array<char, 16> buffer{};
  std::copy_n(name.begin(), std::min(name.size(), buffer.size() - 1),
              buffer.begin());
#ifdef __APPLE__
  pthread_setname_np(buffer.data());
#else
  pthread_setname_np(pthread_self(), buffer.data());
#endif
  // Short enough not to allocate
  this_thread().name = buffer.data();
}

severity_level parse_level(std::string_view text) {
  // In any case of the letters
  auto is_name = [text](std::string_view name) {
    return text.size() == name.size() &&
           std::equal(text.begin(), text.end(), name.begin(),
                      [](char a, char b) {
                        return std::toupper(static_cast<unsigned char>(a)) == b;
                      });
  };
  for (std::size_t i = 0; i < level_names.size(); ++i) {
    if (is_name(level_names[i]) || text == std::to_string(i)) {
      return static_cast<severity_level>(i);
    }
  }
  if (is_name("WARN")) { // as in the macro WARN() and in JSON output
    return warning;
  }
  throw std::invalid_argument("invalid log level: " + std::string(text));
}

Format parse_format(std::string_view text) {
  if (text == "text") {
    return Format::text;
  }
  if (text == "json") {
    return Format::json;
  }
  throw std::invalid_argument("invalid log format: " + std::string(text));
}

Options options_from_environment(Options options) {
  auto apply = [](char const* name, auto const& set) {
    char const* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
      return;
    }
    try {
      set(value);
    } catch (std::invalid_argument const& e) {
      std::fprintf(stderr, "fles_logging: ignoring %s: %s\n", name, e.what());
    }
  };
  apply("CBM_LOG_LEVEL",
        [&](char const* v) { options.level = parse_level(v); });
  apply("CBM_LOG_FORMAT",
        [&](char const* v) { options.format = parse_format(v); });
  apply("CBM_LOG_FILE", [&](char const* v) { options.file = v; });
  apply("CBM_LOG_FILE_LEVEL",
        [&](char const* v) { options.file_level = parse_level(v); });
  return options;
}

void setup(Options const& options) { core().setup(options); }

void on_setup(std::function<void()> callback) {
  core().on_setup(std::move(callback));
}

void flush() noexcept {
  try {
    core().drain();
  } catch (...) { // NOLINT(bugprone-empty-catch): logging must not throw
  }
}

bool enabled(severity_level level) noexcept {
  int minimum = g_minimum.load(std::memory_order_relaxed);
  if (minimum < 0) {
    try {
      core();       // sets up the outputs from the environment
    } catch (...) { // NOLINT(bugprone-empty-catch): logging must not throw
    }
    minimum = g_minimum.load(std::memory_order_relaxed);
  }
  return static_cast<int>(level) >= minimum;
}

void write(severity_level level,
           std::string message,
           Attributes attributes) noexcept {
  try {
    if (!enabled(level)) {
      return;
    }
    core().submit(Record(level, std::move(message), std::move(attributes)));
  } catch (...) { // NOLINT(bugprone-empty-catch): logging must not throw
  }
}

namespace detail {

void write_unformatted(severity_level level, Attributes attributes) noexcept {
  try {
    std::string reason = "unknown exception";
    try {
      throw; // the exception that the caller is handling
    } catch (std::exception const& e) {
      reason = e.what();
    } catch (...) { // NOLINT(bugprone-empty-catch): the reason stays unknown
    }
    attributes.fields.push_back(field("format_error", reason));
    std::string message(attributes.message_template);
    write(level, std::move(message), std::move(attributes));
  } catch (...) { // NOLINT(bugprone-empty-catch): logging must not throw
  }
}

LogStreambuf::~LogStreambuf() { LogStreambuf::sync(); }

LogStreambuf::int_type LogStreambuf::overflow(int_type ch) {
  if (!traits_type::eq_int_type(ch, traits_type::eof())) {
    buffer_.push_back(traits_type::to_char_type(ch));
  }
  return traits_type::not_eof(ch);
}

std::streamsize LogStreambuf::xsputn(char_type const* s, std::streamsize n) {
  buffer_.append(s, static_cast<std::size_t>(n));
  return n;
}

int LogStreambuf::sync() {
  if (!buffer_.empty()) {
    std::string_view message = buffer_;
    // don't log the trailing new line, each message ends one anyway
    if (message.back() == '\n') {
      message.remove_suffix(1);
    }
    write(level_, std::string(message), {});
    buffer_.clear();
  }
  return 0;
}

} // namespace detail
} // namespace logging
