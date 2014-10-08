// Copyright 2014 Jan de Cuveland <cmail@cuveland.de>
#include "log.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup.hpp>

#define __ansi(code_m) "\033[" code_m "m"

#define __ansi_color_black "0"
#define __ansi_color_red "1"
#define __ansi_color_green "2"
#define __ansi_color_yellow "3"
#define __ansi_color_blue "4"
#define __ansi_color_magenta "5"
#define __ansi_color_cyan "6"
#define __ansi_color_white "7"
#define __ansi_color_default "9"

#define __ansi_color_fg_normal(color_m) "3" color_m
#define __ansi_color_bg_normal(color_m) "4" color_m
#define __ansi_color_fg_bright(color_m) "9" color_m
#define __ansi_color_bg_bright(color_m) "10" color_m

#define __ansi_normal "0"
#define __ansi_bold "1"
#define __ansi_faint "2"
#define __ansi_italic "3"
#define __ansi_underlined "4"
#define __ansi_negative "7"
#define __ansi_crossed "9"

namespace logging
{
std::string fancy_icon(severity_level level)
{
    static const char* icons[] = {u8"\U0001F463", // alternative: 2767
                                  u8"\U0001F41B", // alternative: 2761
                                  u8"\u2139",     u8"\u26a0", u8"\u2718",
                                  u8"\U0001F480" // alternative: 26A1, 2762
    };

    if (static_cast<std::size_t>(level) < sizeof(icons) / sizeof(*icons))
        return icons[level];
    else
        return boost::lexical_cast<std::string>(static_cast<int>(level));
}
}

std::ostream& operator<<(std::ostream& strm, severity_level level)
{
    static const char* strings[] = {"TRACE",   "DEBUG", "INFO",
                                    "WARNING", "ERROR", "FATAL"};

    if (static_cast<std::size_t>(level) < sizeof(strings) / sizeof(*strings))
        strm << strings[level];
    else
        strm << static_cast<int>(level);

    return strm;
}

boost::log::formatting_ostream& operator<<(
    boost::log::formatting_ostream& strm,
    boost::log::to_log_manip<severity_level,
                             logging::severity_with_color_tag> const& manip)
{
    static const char* colors[] = {
        __ansi(__ansi_faint),
        __ansi(__ansi_color_fg_normal(__ansi_color_green) ";" __ansi_faint),
        __ansi(__ansi_color_fg_normal(__ansi_color_blue)),
        __ansi(__ansi_color_fg_normal(__ansi_color_yellow)),
        __ansi(__ansi_color_fg_normal(__ansi_color_red)),
        __ansi(
            __ansi_color_bg_bright(__ansi_color_default) ";" __ansi_color_fg_normal(
                __ansi_color_red) ";" __ansi_negative)};

    severity_level level = manip.get();

    if (static_cast<std::size_t>(level) < sizeof(colors) / sizeof(*colors))
        strm << colors[level] << level << ":" << __ansi(__ansi_normal);
    else
        strm << level << ":";

    return strm;
}

BOOST_LOG_GLOBAL_LOGGER_INIT(
    g_logger, boost::log::sources::severity_logger_mt<severity_level>)
{
    boost::log::sources::severity_logger_mt<severity_level> lg;
    lg.add_attribute(boost::log::aux::default_attribute_names::line_id(),
                     boost::log::attributes::counter<unsigned int>(1));
    lg.add_attribute(boost::log::aux::default_attribute_names::timestamp(),
                     boost::log::attributes::local_clock());
    lg.add_attribute(boost::log::aux::default_attribute_names::process_id(),
                     boost::log::attributes::current_process_id());
    lg.add_attribute(boost::log::aux::default_attribute_names::thread_id(),
                     boost::log::attributes::current_thread_id());
    return lg;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level)
#pragma GCC diagnostic pop

namespace logging
{

void add_console(severity_level minimum_severity)
{
    boost::log::formatter console_formatter =
        boost::log::expressions::stream
        << __ansi(__ansi_color_fg_normal(__ansi_color_green)) "["
        << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
               "TimeStamp", "%H:%M:%S") << "]" __ansi(__ansi_normal) " "
        << boost::log::expressions::attr<severity_level,
                                         severity_with_color_tag>("Severity")
        << " " << boost::log::expressions::message;

    auto console_sink = boost::log::add_console_log();
    console_sink->set_formatter(console_formatter);
    console_sink->set_filter(severity >= minimum_severity);
}

void add_file(std::string filename, severity_level minimum_severity)
{
    boost::log::formatter file_formatter =
        boost::log::expressions::stream
        << "["
        << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
               "TimeStamp", "%Y-%m-%d %H:%M:%S") << "] "
        << boost::log::expressions::attr<severity_level>("Severity") << ": "
        << boost::log::expressions::message;

    auto file_sink = boost::log::add_file_log(filename);
    file_sink->set_formatter(file_formatter);
    file_sink->set_filter(severity >= minimum_severity);
}
}
