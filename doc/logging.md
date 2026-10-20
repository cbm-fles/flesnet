# Logging

All flesnet applications log through libfles_logging (`lib/logging`). It
writes text to stderr and, optionally, one JSON object per line to a file.

## Options

Every application takes the same options:

| Option | Environment variable | Meaning |
|---|---|---|
| `-l`, `--log-level LEVEL` | `CBM_LOG_LEVEL` | lowest level on stderr (default: `status`) |
| `--log-format FORMAT` | `CBM_LOG_FORMAT` | `text` or `json` on stderr (default: `text`) |
| `--log-file FILENAME` | `CBM_LOG_FILE` | also append JSON lines to this file |
| `--log-file-level LEVEL` | `CBM_LOG_FILE_LEVEL` | lowest level in the file (default: the stderr level) |

A level is one of `trace`, `debug`, `status`, `info`, `warning` (or `warn`),
`error`, `fatal`, or its number 0-6. An option on the command line takes
precedence over the environment variable. The file is appended to, never
truncated; flesctl gives each process a file of its own.

## Text output

```
[17:48:42] STATUS: Connected to tsmanager
[17:48:43] WARNING: ts12345|Build timeout (after 1002 ms)
[17:48:44] WARNING: ts..123|Build timeout (after 1001 ms)
```

A message about one timeslice starts with its id, shortened to its last three
digits beyond six digits. On a terminal, the lines are coloured by level.

## JSON output

The field names follow the DCA logger, so that both can share one log store.

```json
{"@timestamp":"2026-10-20T15:48:44.303647Z","host":"node01","service":"tsbuilder","thread":"tsbuilder","tid":649694,"severity_code":13,"severity_text":"WARN","timeslice":17900000123,"message":"Build timeout (after 1001 ms)","message_template":"Build timeout (after {} ms)","data":{"build_time_ms":1001},"file_name":"TsBuilder.cpp","line":249,"function":"check_for_timeout","pid":649694,"version":"0.0~git20261020.1737.75597bf9"}
```

| Field | Content |
|---|---|
| `@timestamp` | UTC, with microseconds |
| `host`, `service`, `pid` | the host, the program name and its process id |
| `thread`, `tid` | the thread's name and its system id |
| `severity_code`, `severity_text` | the level, as in OpenTelemetry (see below) |
| `timeslice` | the id of the timeslice the message is about, if any |
| `message` | the text of the message |
| `message_template` | its format string, if it has a meaningful one; it identifies the kind of message |
| `data` | the fields of the message, with their types |
| `file_name`, `line`, `function` | where the message was logged |
| `version` | the software version, including the git commit |

The levels map to OpenTelemetry severities:

| Level | `severity_code` | `severity_text` |
|---|---|---|
| `trace` | 1 | `TRACE` |
| `debug` | 5 | `DEBUG` |
| `status` | 9 | `INFO` |
| `info` | 11 | `NOTICE` |
| `warning` | 13 | `WARN` |
| `error` | 17 | `ERROR` |
| `fatal` | 21 | `FATAL` |

## Logging in code

```cpp
#include "log_short.hpp"

INFO("Connected to {} senders", n);
TS_WARN(id, "Build timeout (after {} ms)",
        logging::field("build_time_ms", ms));
STATUS("Done", logging::field("n_timeslices", n_ts));
```

- `LF(level, format, args...)` and `LF_TS(level, id, format, args...)` are the
  format-style calls; `log_short.hpp` adds `TRACE()` ... `FATAL()` and
  `TS_TRACE()` ... `TS_FATAL()` for them. Other libraries define names like
  `DEBUG` as well, so headers that others include use `LF()` and include
  `log.hpp` only.
- The format string is checked at compile time. Formatting uses `<format>`,
  or `{fmt}` where that is missing, e.g. before C++20.
- `logging::field("key", value)` passes a number, bool or string as a typed
  field in `data`. As a placeholder argument it also prints its value.
- `L_(level) << ...` is the stream interface.
- The arguments are only evaluated if the message is logged.
  `logging::enabled(level)` tells whether it would be.
- `logging::set_thread_name()` names the calling thread for its messages and
  for tools like `top`.

Applications set up logging from their options with
`logging::add_program_options()` and `logging::setup(logging::options_from(vm))`
(`log_program_options.hpp`), or in flesnet with `fles::log_options(vm)`
(`config/CommonOptions.hpp`), which adds the version.

## Behaviour

- A writer thread writes the messages, so logging does not wait for the
  outputs. If more than about 100 MB of messages are waiting, further ones are
  dropped, and the number of dropped messages is logged later.
- The writer collects messages for up to 10 ms. Errors make it write at once.
  A fatal message waits until it is written, but at most 2 s.
- Text on a terminal is written by the thread that logs it, so that it stays
  in order with other terminal output.
- At exit, the queued messages are written. Messages logged after that, e.g.
  from static destructors, are written by the thread that logs them.
- A child process made by `fork()` cannot log; it is expected to call `exec`.
- libfles_logging is a shared library, so that a process has one instance of
  it, also when several shared libraries log.

## Bridging other logging systems

`logging::write(level, message, attributes)` logs a finished message, with
a source location, fields or a timeslice id in `logging::Attributes`; it
copies everything it needs. `logging::on_setup(callback)` calls the callback
now and after each `logging::setup()`, e.g. to pass the levels on to the
other system. cbmroot uses this for FairLogger and ROOT messages.
