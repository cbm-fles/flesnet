/// @cond EINHARD
/**
 * @file
 *
 * This is the main include file for Einhard.
 *
 * Copyright 2010 Matthias Bach <marix@marix.org>
 *
 * This file is part of Einhard.
 *
 * Einhard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Einhard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Einhard.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>

// This C header is sadly required to check whether writing to a terminal or a file
#include <stdio.h>

/**
 * This namespace contains all objects required for logging using Einhard.
 */
namespace einhard {
/**
 * Version string of the Einhard library
 */
static char const VERSION[] = "0.3+";

/**
 * Specification of the message severity.
 *
 * In output each level automatically includes the higher levels.
 */
enum LogLevel { ALL, /**< Log all message */
                TRACE, /**< The lowes severity for messages describing the program flow */
                DEBUG, /**< Debug messages */
                INFO, /**< Messages of informational nature, expected processing time e.g. */
                WARN, /**< Warning messages */
                ERROR, /**< Non-fatal errors */
                FATAL, /**< Messages that indicate terminal application failure */
                OFF /**< If selected no messages will be output */
              };

/**
 * Retrieve a human readable representation of the given log level value.
 */
inline char const* getLogLevelString(const LogLevel level);

/**
 * A stream modifier that allows to colorize the log output.
 */
template<typename Parent> struct Color {
    bool reset;
    Color() : reset(true) {}
    Color<Parent> operator~() const {
        Color<Parent> tmp(*this);
        tmp.reset = false;
        return tmp;
    }
    char const* ansiCode() const {
        return Parent::ANSI();
    }
    bool resetColor() const {
        return reset;
    }
};
#define _COLOR(name, code) \
    struct _##name { static char const * ANSI(); }; \
    inline char const * _##name::ANSI() { return "\33[" code "m"; } \
    typedef Color<_##name> name

_COLOR(DGray,   "01;30");
_COLOR(Black,   "00;30");
_COLOR(Red,     "01;31");
_COLOR(DRed,    "00;31");
_COLOR(Green,   "01;32");
_COLOR(DGreen,  "00;32");
_COLOR(Yellow,  "01;33");
_COLOR(Orange,  "00;33");
_COLOR(Blue,    "01;34");
_COLOR(DBlue,   "00;34");
_COLOR(Magenta, "01;35");
_COLOR(DMagenta, "00;35");
_COLOR(Cyan,    "01;36");
_COLOR(DCyan,   "00;36");
_COLOR(White,   "01;37");
_COLOR(Gray,    "00;37");
_COLOR(NoColor, "0");
#undef _COLOR

#define ANSI_COLOR_WARN  _Orange::ANSI()
#define ANSI_COLOR_ERROR _DRed::ANSI()
#define ANSI_COLOR_FATAL _DRed::ANSI()
#define ANSI_COLOR_INFO  _DGreen::ANSI()
#define ANSI_COLOR_DEBUG _DBlue::ANSI()
#define ANSI_COLOR_CLEAR _NoColor::ANSI()

/**
 * A wrapper for the output stream taking care proper formatting and colorization of the output.
 */
template<LogLevel VERBOSITY, bool THREADSAFE> class OutputFormatter {
private:
    // The output stream to print to
    std::ostream* const out;
    std::ostream* const real_out;  // required for threadsafe
    // Whether to colorize the output
    bool const colorize;
    mutable bool resetColor;

public:
    OutputFormatter(std::ostream* const _out, bool const colorize)
        : out(THREADSAFE && _out ? new std::ostringstream() : _out), real_out(_out), colorize(colorize),
          resetColor(false) {
        if (out != 0) {
            // Figure out current time
            time_t rawtime;
            tm* timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);

            if (colorize) {
                // set color according to log level
                switch (VERBOSITY) {
                case WARN:
                    *out << ANSI_COLOR_WARN;
                    break;
                case ERROR:
                    *out << ANSI_COLOR_ERROR;
                    break;
                case FATAL:
                    *out << ANSI_COLOR_FATAL;
                    break;
                case INFO:
                    *out << ANSI_COLOR_INFO;
                    break;
                case DEBUG:
                    *out << ANSI_COLOR_DEBUG;
                    break;
                default:
                    // in other cases we leave the default color
                    break;
                }
            }

            // output it
            *out << '[';
            *out << std::setfill('0') << std::setw(2) << timeinfo->tm_hour;
            *out << ':';
            *out << std::setfill('0') << std::setw(2) << timeinfo->tm_min;
            *out << ':';
            *out << std::setfill('0') << std::setw(2) << timeinfo->tm_sec;
            *out << ']';
            // TODO would be good to have this at least .01 seconds
            // for non-console output pure timestamp would probably be better

            // output the log level of the message
            *out << ' ' << getLogLevelString(VERBOSITY) << ": ";

            if (colorize) {
                // reset color according to log level
                switch (VERBOSITY) {
                case INFO:
                case DEBUG:
                case WARN:
                case ERROR:
                case FATAL:
                    *out << ANSI_COLOR_CLEAR;
                    break;
                default:
                    // in other cases color is still default anyways
                    break;
                }
            }
        }
    }

    template<typename T> inline const OutputFormatter<VERBOSITY, THREADSAFE>& operator<<(const Color<T>& col) const {
        if (out && colorize) {
            *out << col.ansiCode();
            resetColor = col.resetColor();
        }
        return *this;
    }

    template<typename T> inline const OutputFormatter<VERBOSITY, THREADSAFE>& operator<<(const T& msg) const {
        // output the log message
        if (out != 0) {
            *out << msg;
            if (resetColor) {
                *out << ANSI_COLOR_CLEAR;
                resetColor = false;
            }
        }

        return *this;
    }

    ~OutputFormatter() {
        if (out != 0) {
            // make sure there is no strange formatting set anymore
            *out << std::resetiosflags(std::ios_base::floatfield  | std::ios_base::basefield
                                       | std::ios_base::adjustfield | std::ios_base::uppercase
                                       | std::ios_base::showpos     | std::ios_base::showpoint
                                       | std::ios_base::showbase    |  std::ios_base::boolalpha);

            *out << std::endl; // TODO this would probably better be only '\n' as to not flush the buffers

            if (THREADSAFE) {
                std::ostringstream* buf = static_cast<std::ostringstream*>(out);
                *real_out << buf->str();
                delete buf;
            }
        }
    }
};

/**
 * A Logger object can be used to output messages to stdout.
 *
 * The Logger object is created with a certain verbosity. All messages of a more verbose
 * LogLevel will be filtered out. The way the class is build this can happen at compile
 * time up to the level restriction given by the template parameter.
 *
 * The class can automatically detect non-tty output and will not colorize output in that case.
 */
template<LogLevel MAX = ALL, bool THREADSAFE = false> class Logger {
private:
    LogLevel verbosity;
    bool colorize;

public:
    /**
     * Create a new Logger object.
     *
     * The object will automatically colorize output on ttys and not colorize output
     * on non ttys.
     */
    Logger(const LogLevel verbosity = WARN) : verbosity(verbosity) {
        // use some, sadly not c++-ways to figure out whether we are writing ot a terminal
        // only colorize when we are writing ot a terminal
        colorize = isatty(fileno(stdout));
    };
    /**
     * Create a new Logger object explicitly selecting whether to colorize the output or not.
     *
     * Be aware that if output colorization is selected output will even be colorized if
     * output is to a non tty.
     */
    Logger(const LogLevel verbosity, const bool colorize) : verbosity(verbosity), colorize(colorize) { };

    /** Access to the trace message stream. */
    inline const OutputFormatter<TRACE, THREADSAFE> trace() const;
    /** Access to the debug message stream. */
    inline const OutputFormatter<DEBUG, THREADSAFE> debug() const;
    /** Access to the info message stream. */
    inline const OutputFormatter<INFO, THREADSAFE> info() const;
    /** Access to the warning message stream. */
    inline const OutputFormatter<WARN, THREADSAFE> warn() const;
    /** Access to the error message stream. */
    inline const OutputFormatter<ERROR, THREADSAFE> error() const;
    /** Access to the fatal message stream. */
    inline const OutputFormatter<FATAL, THREADSAFE> fatal() const;

    /** Check whether trace messages will be output */
    inline bool beTrace() const;
    /** Check whether debug messages will be output */
    inline bool beDebug() const;
    /** Check whether info messages will be output */
    inline bool beInfo() const;
    /** Check whether warn messages will be output */
    inline bool beWarn() const;
    /** Check whether error messages will be output */
    inline bool beError() const;
    /** Check whether fatal messages will be output */
    inline bool beFatal() const;

    /** Modify the verbosity of the Logger.
     *
     * Be aware that the verbosity can not be increased over the level given by the template
     * parameter
     */
    inline void setVerbosity(LogLevel verbosity) {
        this->verbosity = verbosity;
    }
    /** Retrieve the current log level.
     *
     * If you want to check whether messages of a certain LogLevel will be output the
     * methos beTrace(), beDebug(), beInfo(), beWarn(), beError() and beFatal() should be
     * preffered.
     */
    inline LogLevel getVerbosity() const {
        return this->verbosity;
    }
    /**
     * Retrieve a human readable representation of the current log level
     */
    inline char const* getVerbosityString() const {
        return getLogLevelString(this->verbosity);
    }
    /**
     * Select whether the output stream should be colorized.
     */
    inline void setColorize(bool colorize) {
        this->colorize = colorize;
    }
    /**
     * Check whether the output stream is colorized.
     */
    inline void getColorize() const {
        return this->colorize;
    }
};

/*
 * IMPLEMENTATIONS
 */

inline char const* getLogLevelString(const LogLevel level)
{
    switch (level) {
    case ALL:
        return "ALL";
    case TRACE:
        return "TRACE";
    case DEBUG:
        return "DEBUG";
    case INFO:
        return "INFO";
    case WARN:
        return "WARNING";
    case ERROR:
        return "ERROR";
    case FATAL:
        return "FATAL";
    case OFF:
        return "OFF";
    default:
        return "--- Something is horribly broken - A value outside the enumation has been given ---";
    }
}

template<LogLevel MAX, bool THREADSAFE> const OutputFormatter<TRACE, THREADSAFE> Logger<MAX, THREADSAFE>::trace() const
{
    if (beTrace())
        return OutputFormatter<TRACE, THREADSAFE>(&std::cout, colorize);
    else
        return OutputFormatter<TRACE, THREADSAFE>(0, colorize);
}

template<LogLevel MAX, bool THREADSAFE> bool Logger<MAX, THREADSAFE>::beTrace() const
{
#ifdef NDEBUG
    return false;
#else
    return (MAX <= TRACE && verbosity <= TRACE);
#endif
}

template<LogLevel MAX, bool THREADSAFE> const OutputFormatter<DEBUG, THREADSAFE> Logger<MAX, THREADSAFE>::debug() const
{
    if (beDebug())
        return OutputFormatter<DEBUG, THREADSAFE>(&std::cout, colorize);
    else
        return OutputFormatter<DEBUG, THREADSAFE>(0, colorize);
}

template<LogLevel MAX, bool THREADSAFE> bool Logger<MAX, THREADSAFE>::beDebug() const
{
#ifdef NDEBUG
    return false;
#else
    return (MAX <= DEBUG && verbosity <= DEBUG);
#endif
}

template<LogLevel MAX, bool THREADSAFE> const OutputFormatter<INFO, THREADSAFE> Logger<MAX, THREADSAFE>::info() const
{
    if (beInfo())
        return OutputFormatter<INFO, THREADSAFE>(&std::cout, colorize);
    else
        return OutputFormatter<INFO, THREADSAFE>(0, colorize);
}

template<LogLevel MAX, bool THREADSAFE> bool Logger<MAX, THREADSAFE>::beInfo() const
{
    return (MAX <= INFO && verbosity <= INFO);
}

template<LogLevel MAX, bool THREADSAFE> const OutputFormatter<WARN, THREADSAFE> Logger<MAX, THREADSAFE>::warn() const
{
    if (beWarn())
        return OutputFormatter<WARN, THREADSAFE>(&std::cout, colorize);
    else
        return OutputFormatter<WARN, THREADSAFE>(0, colorize);
}

template<LogLevel MAX, bool THREADSAFE> bool Logger<MAX, THREADSAFE>::beWarn() const
{
    return (MAX <= WARN && verbosity <= WARN);
}

template<LogLevel MAX, bool THREADSAFE> const OutputFormatter<ERROR, THREADSAFE> Logger<MAX, THREADSAFE>::error() const
{
    if (beError())
        return OutputFormatter<ERROR, THREADSAFE>(&std::cout, colorize);
    else
        return OutputFormatter<ERROR, THREADSAFE>(0, colorize);
}

template<LogLevel MAX, bool THREADSAFE> bool Logger<MAX, THREADSAFE>::beError() const
{
    return (MAX <= ERROR && verbosity <= ERROR);
}

template<LogLevel MAX, bool THREADSAFE> const OutputFormatter<FATAL, THREADSAFE> Logger<MAX, THREADSAFE>::fatal() const
{
    if (beFatal())
        return OutputFormatter<FATAL, THREADSAFE>(&std::cout, colorize);
    else
        return OutputFormatter<FATAL, THREADSAFE>(0, colorize);
}

template<LogLevel MAX, bool THREADSAFE> bool Logger<MAX, THREADSAFE>::beFatal() const
{
    return (MAX <= FATAL && verbosity <= FATAL);
}

}

#undef ANSI_COLOR_WARN
#undef ANSI_COLOR_ERROR
#undef ANSI_COLOR_FATAL
#undef ANSI_COLOR_INFO
#undef ANSI_COLOR_DEBUG
#undef ANSI_COLOR_CLEAR

#define EINHARD_STREAM(arg) \
template<einhard::LogLevel VERBOSITY, bool THREADSAFE> \
inline const einhard::OutputFormatter<VERBOSITY, THREADSAFE>& operator<<(const einhard::OutputFormatter<VERBOSITY, THREADSAFE> &out, arg)
#define EINHARD_STREAM_T1(T1, arg) \
template<einhard::LogLevel VERBOSITY, bool THREADSAFE, T1> \
inline const einhard::OutputFormatter<VERBOSITY, THREADSAFE>& operator<<(const einhard::OutputFormatter<VERBOSITY, THREADSAFE> &out, arg)
#define EINHARD_STREAM_T2(T1, T2, arg1, arg2) \
template<einhard::LogLevel VERBOSITY, bool THREADSAFE, T1, T2> \
inline const einhard::OutputFormatter<VERBOSITY, THREADSAFE>& operator<<(const einhard::OutputFormatter<VERBOSITY, THREADSAFE> &out, arg1, arg2)

// vim: ts=4 sw=4 tw=100 noet
/// @endcond
