// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

void Parameters::parse_options(int argc, char* argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()("version,V", "print version string")(
        "help,h", "produce help message")(
        "client-index,c", po::value<int32_t>(&_client_index),
        "index of this executable in the list of processor tasks")(
        "analyze-pattern,a", po::value<bool>(&_analyze)->implicit_value(true),
        "enable/disable pattern check")(
        "verbose,v", po::value<size_t>(&_verbosity), "set output verbosity")(
        "shm-identifier,s", po::value<std::string>(&_shm_identifier),
        "shared memory identifier used for receiving timeslices")(
        "input-archive,i", po::value<std::string>(&_input_archive),
        "name of an input file archive to read")(
        "output-archive,o", po::value<std::string>(&_output_archive),
        "name of an output file archive to write")(
        "publish,P", po::value<std::string>(&_publish_address)
                         ->implicit_value("tcp://*:5556"),
        "enable timeslice publisher on given address")(
        "subscribe,S", po::value<std::string>(&_subscribe_address)
                           ->implicit_value("tcp://localhost:5556"),
        "subscribe to timeslice publisher on given address");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(EXIT_SUCCESS);
    }

    if (vm.count("version")) {
        std::cout << "tsclient, version 0.0" << std::endl;
        exit(EXIT_SUCCESS);
    }

    int input_sources = vm.count("shm-identifier") + vm.count("input-archive") +
                        vm.count("subscribe");
    if (input_sources == 0)
        throw ParametersException("no input source specified");
    if (input_sources > 1)
        throw ParametersException("more than one input source specified");
}
