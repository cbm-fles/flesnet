// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "log.hpp"
#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

void Parameters::parse_options(int argc, char* argv[])
{
    unsigned log_level = 2;
    std::string log_file;

    po::options_description desc("Allowed options");
    auto desc_add = desc.add_options();
    desc_add("version,V", "print version string");
    desc_add("help,h", "produce help message");
    desc_add("log-level,l", po::value<unsigned>(&log_level),
             "set the log level (default:2, all:0)");
    desc_add("log-file,L", po::value<std::string>(&log_file),
             "name of target log file");
    desc_add("client-index,c", po::value<int32_t>(&client_index_),
             "index of this executable in the list of processor tasks");
    desc_add("analyze-pattern,a",
             po::value<bool>(&analyze_)->implicit_value(true),
             "enable/disable pattern check");
    desc_add("benchmark,b", po::value<bool>(&benchmark_)->implicit_value(true),
             "run benchmark test only");
    desc_add("verbose,v", po::value<size_t>(&verbosity_),
             "set output verbosity");
    desc_add("shm-identifier,s", po::value<std::string>(&shm_identifier_),
             "shared memory identifier used for receiving timeslices");
    desc_add("input-archive,i", po::value<std::string>(&input_archive_),
             "name of an input file archive to read");
    desc_add(
        "input-archive-cycles", po::value<uint64_t>(&input_archive_cycles_),
        "repeat reading input archive in a loop (for performance testing)");
    desc_add("output-archive,o", po::value<std::string>(&output_archive_),
             "name of an output file archive to write");
    desc_add("output-archive-items", po::value<size_t>(&output_archive_items_),
             "limit number of timeslices per file to given number, create "
             "sequence of output archive files (use placeholder %n in "
             "output-archive parameter)");
    desc_add("output-archive-bytes", po::value<size_t>(&output_archive_bytes_),
             "limit number of bytes per file to given number, create "
             "sequence of output archive files (use placeholder %n in "
             "output-archive parameter)");
    desc_add("publish,P", po::value<std::string>(&publish_address_)
                              ->implicit_value("tcp://*:5556"),
             "enable timeslice publisher on given address");
    desc_add("subscribe,S", po::value<std::string>(&subscribe_address_)
                                ->implicit_value("tcp://localhost:5556"),
             "subscribe to timeslice publisher on given address");
    desc_add("maximum-number,n", po::value<uint64_t>(&maximum_number_),
             "set the maximum number of microslices to process (default: "
             "unlimited)");
    desc_add("rate-limit", po::value<double>(&rate_limit_),
             "limit the item rate to given frequency (in Hz)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help") != 0u) {
        std::cout << desc << std::endl;
        exit(EXIT_SUCCESS);
    }

    if (vm.count("version") != 0u) {
        std::cout << "tsclient, version 0.0" << std::endl;
        exit(EXIT_SUCCESS);
    }

    logging::add_console(static_cast<severity_level>(log_level));
    if (vm.count("log-file")) {
        L_(info) << "Logging output to " << log_file;
        logging::add_file(log_file, static_cast<severity_level>(log_level));
    }

    size_t input_sources = vm.count("shm-identifier") +
                           vm.count("input-archive") + vm.count("subscribe");
    if (input_sources == 0 && !benchmark_) {
        throw ParametersException("no input source specified");
    }
    if (input_sources > 1) {
        throw ParametersException("more than one input source specified");
    }
}
