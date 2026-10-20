// Copyright 2026 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Short names for the format-style log calls of log.hpp.
///
/// INFO("{} hits in {}", n, det) is LF(info, ...), and TS_INFO(id, "Released")
/// is LF_TS(info, id, ...). Other libraries define names like DEBUG as well,
/// so this header is for source files, not for headers that others include.
#pragma once

#include "log.hpp"

#define TRACE(...) LF(trace, __VA_ARGS__)
#define DEBUG(...) LF(debug, __VA_ARGS__)
#define STATUS(...) LF(status, __VA_ARGS__)
#define INFO(...) LF(info, __VA_ARGS__)
#define WARN(...) LF(warning, __VA_ARGS__)
#define ERROR(...) LF(error, __VA_ARGS__)
#define FATAL(...) LF(fatal, __VA_ARGS__)

#define TS_TRACE(id, ...) LF_TS(trace, id, __VA_ARGS__)
#define TS_DEBUG(id, ...) LF_TS(debug, id, __VA_ARGS__)
#define TS_STATUS(id, ...) LF_TS(status, id, __VA_ARGS__)
#define TS_INFO(id, ...) LF_TS(info, id, __VA_ARGS__)
#define TS_WARN(id, ...) LF_TS(warning, id, __VA_ARGS__)
#define TS_ERROR(id, ...) LF_TS(error, id, __VA_ARGS__)
#define TS_FATAL(id, ...) LF_TS(fatal, id, __VA_ARGS__)
