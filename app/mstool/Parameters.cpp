// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "log.hpp"
#include <iostream>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

void Parameters::parse_options(int argc, char* argv[])
{
    unsigned log_level = 2;

    po::options_description desc("Allowed options");
    desc.add_options()("version,V", "print version string")(
        "help,h",
        "produce help message")("log-level,l", po::value<unsigned>(&log_level),
                                "set the log level (default:2, all:0)")(
        "analyze-pattern,a", po::value<bool>(&analyze_)->implicit_value(true),
        "enable/disable pattern check")(
        "pattern-generator,p", po::value<uint32_t>(&pattern_generator_type_),
        "use pattern generator to produce timeslices")(
        "shm-channel,c", po::value<size_t>(&shared_memory_channel_),
        "use given shared memory channel as data source")(
        "input-archive,i", po::value<std::string>(&input_archive_),
        "name of an input file archive to read")(
        "output-archive,o", po::value<std::string>(&output_archive_),
        "name of an output file archive to write")(
        "maximum-number,n", po::value<uint64_t>(&maximum_number_),
        "set the maximum number of microslices to process (default: "
        "unlimited)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(EXIT_SUCCESS);
    }

    if (vm.count("version")) {
        std::cout << "mstool, version 0.0" << std::endl;
        exit(EXIT_SUCCESS);
    }

    logging::add_console(static_cast<severity_level>(log_level));

    if (vm.count("pattern-generator")) {
        use_pattern_generator_ = true;
    }

    if (vm.count("shm-channel")) {
        use_shared_memory_ = true;
    }

    size_t input_sources = vm.count("pattern-generator") +
                           vm.count("input-archive") + vm.count("shm-channel");
    if (input_sources == 0)
        throw ParametersException("no input source specified");
    if (input_sources > 1)
        throw ParametersException("more than one input source specified");
}
