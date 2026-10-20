// Copyright 2014, 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Process-wide logging, implemented in libfles_logging.
///
/// The stream interface L_() and everything in namespace logging only use the
/// standard library and work from C++17 code, ROOT/cling and GPU builds. The
/// format-style macros at the end use <format> or, where that is missing,
/// {fmt}. log_short.hpp adds short names for them, e.g. INFO().
///
/// A child process made by fork() cannot log; it is expected to call exec.
#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <optional>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define FLES_LOGGING_EXPORT __attribute__((visibility("default")))
#else
#define FLES_LOGGING_EXPORT
#endif

enum severity_level { trace, debug, status, info, warning, error, fatal };

FLES_LOGGING_EXPORT std::ostream& operator<<(std::ostream& strm,
                                             severity_level level);

namespace logging {

enum class Format { text, json };

/// A named value that a message carries in addition to its text. JSON output
/// lists the fields of a message as "data".
struct Field {
  using Value =
      std::variant<std::int64_t, std::uint64_t, double, bool, std::string>;
  std::string key;
  Value value;
};

/// field("n_hits", n), e.g. as an extra argument of a format-style log call
template <typename T> Field field(std::string key, T const& value) {
  if constexpr (std::is_same_v<T, bool>) {
    return {std::move(key), value};
  } else if constexpr (std::is_same_v<T, char>) {
    return {std::move(key), std::string(1, value)};
  } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
    return {std::move(key), static_cast<std::int64_t>(value)};
  } else if constexpr (std::is_integral_v<T>) {
    return {std::move(key), static_cast<std::uint64_t>(value)};
  } else if constexpr (std::is_floating_point_v<T>) {
    return {std::move(key), static_cast<double>(value)};
  } else if constexpr (std::is_convertible_v<T const&, char const*>) {
    char const* const text = value; // a null pointer gives an empty string
    return {std::move(key), std::string(text != nullptr ? text : "")};
  } else {
    static_assert(std::is_constructible_v<std::string, T const&>,
                  "logging::field() takes a number, a bool, a char or a "
                  "string; convert other values first");
    return {std::move(key), std::string(value)};
  }
}

/// Where log messages go: stderr, and optionally a file with JSON lines.
/// A writer thread writes them, errors without delay. A fatal message waits
/// up to 2 s until it is written. Text on a terminal is written by the thread
/// that logs it.
struct Options {
  severity_level level = status; ///< on stderr
  Format format = Format::text;  ///< on stderr; text is coloured on a tty
  std::string file;              ///< JSON lines, appended; empty: none
  std::optional<severity_level> file_level; ///< default: level
  /// Extra fields in every JSON line, e.g. field("run", 1234). Their keys
  /// must differ from those that every line has, e.g. "host" or "message".
  std::vector<Field> context;
};

/// The given options, overridden by the environment variables CBM_LOG_LEVEL,
/// CBM_LOG_FORMAT, CBM_LOG_FILE and CBM_LOG_FILE_LEVEL where set. Invalid
/// values are reported on stderr and ignored.
FLES_LOGGING_EXPORT Options options_from_environment(Options options = {});

/// A level name (trace, debug, status, info, warning or warn, error, fatal)
/// or its number (0-6). Throws std::invalid_argument otherwise.
FLES_LOGGING_EXPORT severity_level parse_level(std::string_view text);
/// "text" or "json". Throws std::invalid_argument otherwise.
FLES_LOGGING_EXPORT Format parse_format(std::string_view text);

/// Replace the outputs. Until the first call, the options come from the
/// environment (see options_from_environment()). Throws and keeps the outputs
/// if the file cannot be opened (std::runtime_error) or a context key is
/// taken (std::invalid_argument).
FLES_LOGGING_EXPORT void setup(Options const& options);

/// Name the calling thread, for its messages and for tools like top. The
/// system keeps 15 characters. A name given by other means, e.g.
/// pthread_setname_np(), reaches the messages only before the first one.
FLES_LOGGING_EXPORT void set_thread_name(std::string_view name) noexcept;

/// Wait until all messages logged so far are written.
FLES_LOGGING_EXPORT void flush() noexcept;

/// True if a message at this level would reach any output.
FLES_LOGGING_EXPORT bool enabled(severity_level level) noexcept;

/// Call `callback` now and after each later setup(). For adapters that must
/// follow the levels, e.g. to filter messages before they are formatted. The
/// callback must not call setup() or on_setup().
FLES_LOGGING_EXPORT void on_setup(std::function<void()> callback);

/// What a message carries besides its level and text
struct Attributes {
  std::string_view file; ///< empty: no source location
  unsigned line = 0;
  std::string_view function;
  std::string_view message_template;      ///< the format string, if any
  std::optional<std::uint64_t> timeslice; ///< the timeslice it is about
  std::vector<Field> fields;
};

/// Log a finished message. This is the entry point for the macros below and
/// for adapters that bridge other logging systems. It copies the strings that
/// the attributes refer to.
FLES_LOGGING_EXPORT void write(severity_level level,
                               std::string message,
                               Attributes attributes) noexcept;

namespace detail {
/// The attributes of a message from one of the macros
inline Attributes
make_attributes(char const* file,
                unsigned line,
                char const* function,
                std::string_view message_template = {},
                std::optional<std::uint64_t> timeslice = std::nullopt) {
  Attributes attributes;
  attributes.file = file;
  attributes.line = line;
  attributes.function = function;
  attributes.message_template = message_template;
  attributes.timeslice = timeslice;
  return attributes;
}

/// Log the format string of a message that could not be formatted, with the
/// reason as the field "format_error". To be called while handling the
/// exception.
FLES_LOGGING_EXPORT void write_unformatted(severity_level level,
                                           Attributes attributes) noexcept;

/// Collects one message written via L_() and logs it on destruction.
class MessageStream {
public:
  MessageStream(severity_level level,
                char const* file,
                unsigned line,
                char const* function)
      : level_(level), line_(line), file_(file), function_(function) {}
  MessageStream(MessageStream const&) = delete;
  MessageStream& operator=(MessageStream const&) = delete;
  ~MessageStream() {
    write(level_, stream_.str(), make_attributes(file_, line_, function_));
  }

  std::ostream& stream() { return stream_; }

private:
  severity_level level_;
  unsigned line_;
  char const* file_;
  char const* function_;
  std::ostringstream stream_;
};

/// Stream buffer that logs its contents on each flush.
class FLES_LOGGING_EXPORT LogStreambuf : public std::streambuf {
public:
  explicit LogStreambuf(severity_level level) : level_(level) {}
  ~LogStreambuf() override;

protected:
  int_type overflow(int_type ch) override;
  std::streamsize xsputn(char_type const* s, std::streamsize n) override;
  int sync() override;

private:
  severity_level level_;
  std::string buffer_;
};
} // namespace detail

/// An std::ostream that logs each flushed chunk (e.g. up to std::endl) as one
/// message, for interfaces that expect a stream.
class OstreamLog {
public:
  OstreamLog(severity_level level) : buffer_(level), stream(&buffer_) {}

private:
  detail::LogStreambuf buffer_;

public:
  std::ostream stream;
};
} // namespace logging

// The macros take the level by its name, e.g. L_(info), and look it up in
// the global namespace, so that a variable called error or info is not in
// the way.
//
// The stream arguments are only evaluated if the message is logged.
#define L_(severity)                                                           \
  for (bool fles_log_enabled_ = ::logging::enabled(::severity);                \
       fles_log_enabled_; fles_log_enabled_ = false)                           \
  ::logging::detail::MessageStream(::severity, __FILE__, __LINE__, __func__)   \
      .stream()

// Format-style calls: LF(info, "{} hits in {}", n, det). Arguments made by
// logging::field() become fields of the message, whether a placeholder
// prints them or not. LF_TS(info, id, "Released") is a message about one
// timeslice: text output shows the shortened id in front of the message
// ("ts..123|Released"), JSON output has the id as the field "timeslice".
//
// They format with <format>. Before C++20 or with a standard library that
// lacks it, {fmt} takes its place if available; define FLES_LOGGING_USE_FMT
// to choose it in any case. FLES_LOGGING_FORMAT_NS is the namespace in use,
// e.g. to define a formatter that works with both.
#if __has_include(<version>)
#include <version>
#endif
#if !defined(FLES_LOGGING_USE_FMT) && defined(__cpp_lib_format)
#include <format>
#define FLES_LOGGING_FORMAT_NS std
#define FLES_LOGGING_FORMAT_STRING(fmt) fmt
#elif __has_include(<fmt/format.h>)
#include <fmt/format.h>
#define FLES_LOGGING_FORMAT_NS fmt
#if defined(FMT_HAS_CONSTEVAL) ||                                              \
    (defined(FMT_USE_CONSTEVAL) && FMT_USE_CONSTEVAL)
// {fmt} checks the format string at compile time. FMT_STRING would do no
// more, and some {fmt} versions reject it with newer compilers.
#define FLES_LOGGING_FORMAT_STRING(fmt) fmt
#else
// Checks the format string at compile time also before C++20
#define FLES_LOGGING_FORMAT_STRING(fmt) FMT_STRING(fmt)
#endif
#endif

#ifdef FLES_LOGGING_FORMAT_NS
/// As a placeholder argument, a field prints its value. It takes no format
/// specification.
template <> struct FLES_LOGGING_FORMAT_NS::formatter<logging::Field> {
  constexpr auto parse(FLES_LOGGING_FORMAT_NS::format_parse_context& ctx) {
    if (ctx.begin() != ctx.end() && *ctx.begin() != '}') {
      throw FLES_LOGGING_FORMAT_NS::format_error(
          "a logging::field() takes no format specification");
    }
    return ctx.begin();
  }
  auto format(logging::Field const& field,
              FLES_LOGGING_FORMAT_NS::format_context& ctx) const {
    return std::visit(
        [&ctx](auto const& value) {
          return FLES_LOGGING_FORMAT_NS::format_to(ctx.out(), "{}", value);
        },
        field.value);
  }
};

namespace logging::detail {

inline void collect_field(std::vector<Field>& fields, Field const& field) {
  fields.push_back(field);
}
template <typename T>
void collect_field(std::vector<Field>& /* fields */, T const& /* other */) {}

// std::type_identity is C++20
template <typename T> struct type_identity {
  using type = T;
};

// Not inlined, to keep the call sites small. The macros pass the format
// string twice: checked against the arguments, and as the first of their
// variable arguments.
template <typename... Args>
[[gnu::noinline]] void write_formatted(
    severity_level level,
    Attributes attributes,
    typename type_identity<
        FLES_LOGGING_FORMAT_NS::format_string<Args const&...>>::type fmt,
    char const* /* fmt */,
    Args const&... args) noexcept {
  std::string message;
  try {
    (collect_field(attributes.fields, args), ...);
    message = FLES_LOGGING_FORMAT_NS::format(fmt, args...);
  } catch (...) {
    write_unformatted(level, std::move(attributes));
    return;
  }
  write(level, std::move(message), std::move(attributes));
}

} // namespace logging::detail

// The format string is the first of the variable arguments, so that the
// macros need no __VA_OPT__, which is C++20.
#define FLES_LOGGING_FIRST(first, ...) first

// The timeslice id and the arguments are only evaluated if the message is
// logged.
#define FLES_LOGGING_LF(severity, timeslice, ...)                              \
  do {                                                                         \
    if (::logging::enabled(severity)) {                                        \
      ::logging::detail::write_formatted(                                      \
          severity,                                                            \
          ::logging::detail::make_attributes(                                  \
              __FILE__, __LINE__, __func__, FLES_LOGGING_FIRST(__VA_ARGS__, ), \
              timeslice),                                                      \
          FLES_LOGGING_FORMAT_STRING(FLES_LOGGING_FIRST(__VA_ARGS__, )),       \
          __VA_ARGS__);                                                        \
    }                                                                          \
  } while (0)

#define LF(severity, ...) FLES_LOGGING_LF(::severity, std::nullopt, __VA_ARGS__)
#define LF_TS(severity, id, ...)                                               \
  FLES_LOGGING_LF(::severity, static_cast<std::uint64_t>(id), __VA_ARGS__)
#endif
