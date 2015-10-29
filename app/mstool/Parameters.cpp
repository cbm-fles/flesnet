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
    auto desc_add = desc.add_options();

    // general options
    desc_add("version,V", "print version string");
    desc_add("help,h", "produce help message");
    desc_add("log-level,l", po::value<unsigned>(&log_level),
             "set the log level (default:2, all:0)");
    desc_add("maximum-number,n", po::value<uint64_t>(&maximum_number),
             "set the maximum number of microslices to process (default: "
             "unlimited)");

    // source selection
    desc_add("pattern-generator,p", po::value<uint32_t>(&pattern_generator),
             "use pattern generator to produce timeslices");
    desc_add("shm-channel,c", po::value<size_t>(&shm_channel),
             "use given shared memory channel as data source");
    desc_add("input-shm", po::value<std::string>(&input_shm),
             "name of a shared memory to use as data source");
    desc_add("input-archive,i", po::value<std::string>(&input_archive),
             "name of an input file archive to read");

    // sink selection
    desc_add("analyze,a", po::value<bool>(&analyze)->implicit_value(true),
             "enable/disable pattern check");
    desc_add("output-shm", po::value<std::string>(&output_shm),
             "name of a shared memory to write to");
    desc_add("output-archive,o", po::value<std::string>(&output_archive),
             "name of an output file archive to write");

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

    use_pattern_generator = vm.count("pattern-generator") != 0;

    size_t input_sources = vm.count("pattern-generator") +
                           vm.count("input-archive") + vm.count("shm-channel");
    if (input_sources == 0)
        throw ParametersException("no input source specified");
    if (input_sources > 1)
        throw ParametersException("more than one input source specified");
}
