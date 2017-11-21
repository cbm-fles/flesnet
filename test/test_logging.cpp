// Copyright 2016 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

#include "log.hpp"
#include <string>

int main()
{
    unsigned log_level = 0; // 0 == all
    std::string log_file = "test_logging.log";

    logging::add_console(static_cast<severity_level>(log_level));
    logging::add_file(log_file, static_cast<severity_level>(log_level));

    L_(trace) << "This is a trace message.";
    L_(debug) << "This is a debug message.";
    L_(status) << "This is a status message.";
    L_(info) << "This is a info message.";
    L_(warning) << "This is warning message.";
    L_(error) << "This is an error message.";
    L_(fatal) << "This is a fatal error message.";

    logging::OstreamLog status_log(status);
    std::ostream& status_stream = status_log.stream;

    status_stream << "This is a status_stream message w/o endline.";
    status_stream << "This is a status_stream message" << std::endl;
    status_stream << "This is the first ...\n";
    status_stream << "... and the second line of a status_stream message."
                  << std::endl;
    status_stream << "This is a status_stream message" << std::endl;

    return 0;
}
