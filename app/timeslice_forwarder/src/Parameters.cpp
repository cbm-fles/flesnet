#include <df/Utils/Utils.hpp>
#include "Parameters.hpp"
#include <boost/program_options.hpp>
#include <cstdlib>
#include <ostream>
#include <string>
#include <iostream>
#include <boost/assign/list_of.hpp>
#include <df/WorkItems/WiConnectorConfig.hpp>
#include <df/WorkItems/WorkItem.hpp>
#include <log.hpp>

using namespace std;
namespace po = boost::program_options;
void Parameters::parse_options(int argc, char** argv) {
    unsigned log_syslog = 2;
    unsigned log_level = 2;
    po::options_description general("General options");
    auto general_add = general.add_options();
    string connectors_option;

    general_add("help,h", "Print this help");
    general_add("log-level,l",
              po::value<unsigned>(&log_level)
                  ->default_value(log_level)
                  ->value_name("<n>"),
              "set the file log level (all:0)");

    general_add("listen-address,A",
        po::value<string>(&listen_addr),
        "Defines interface listen address to be used by other nodes to establish a connnection '-A <own_IB_ip>:<port>'. \
        If you start multiple forwarder nodes on the same host, make sure that the ports are not in conflict.");

    general_add("central-manager,c",
        po::value<string>(&central_manager_listen_addr),
        "IP and port of the IB interface of the CM manager '-c <CM_IB_ip>:<port>'");

    general_add("monitor,m",
        po::value<string>(&monitoring_uri),
        "monitoring uri influxdb2:host:timeslice_forwarder_state:[auth_token]");

    general_add("node-id,N",
        po::value<uint32_t>(&node_id),
        "Sets the node ID for this node. Has to be unique for sender and reciver and can simply be an increasing number.");

    general_add("output-uri,o",
        po::value<string>(&output_uri),
        "If defined this node will start as the a receiver. !Currently only SHM supported");

    general_add("input-uri,i",
        po::value<string>(&input_uri),
        "If defined this node will start as the a sender. !Currently only SHM supported");

    general_add("log-syslog,S",
              po::value<unsigned>(&log_syslog)
                  ->implicit_value(log_syslog)
                  ->value_name("<n>"),
              "enable logging to syslog at given log level");
    general_add("timeslice-size",
             po::value<uint32_t>(&timeslice_size)
                 ->default_value(timeslice_size)
                 ->value_name("<n>"),
             "set the global timeslice size in number of microslices");
    stringstream desc_sstr;
    desc_sstr << endl
        << "NOTE: The TS Sender node expects the timeslices to be provided using a timeslice client via SHM." << endl
        << "Start Central Manager: " << endl
        << "timeslice_forwarder --central-manager <CM_IB_ip>:<CM_port>" << endl << endl
        << "Start TS Sender Node:" << endl
        << "timeslice_forwarder --node-id <n> -A <own_IB_ip>:<own_port> --central-manager <CM_IB_ip>:<CM_port> -i <shm uri like in flesnet>" << endl << endl
        << "Start TS Receiver Node:" << endl
        << "timeslice_forwarder --node-id <n> -A <own_IB_ip>:<own_port> --central-manager <CM_IB_ip>:<CM_port> -o <shm uri like in flesnet>"
        << endl << endl;

    po::variables_map vm;
    po::options_description desc(desc_sstr.str());
    desc
        .add(general);

    po::store(po::parse_command_line(argc, argv, general), vm);
    po::notify(vm);

    if (vm.count("help") != 0) {
        cout << desc << endl;
        exit(EXIT_SUCCESS);
    }

    logging::add_console(static_cast<severity_level>(log_level));
    if (vm.count("log-syslog") != 0u) {
        logging::add_syslog(logging::syslog::local0,
                            static_cast<severity_level>(log_syslog));
    }

    if (vm.count("input-uri") != 0u && vm.count("output-uri") != 0u) {
        cout << "! input-uri AND output-uri are defined - only set one of them" << endl << endl;
        cout << desc << endl;
        exit(EXIT_SUCCESS);
    } else if (vm.count("input-uri") == 0u && vm.count("output-uri") == 0u) {
        role = CentralManager;
    } else if (vm.count("input-uri") != 0u) {
        role = Sender;
    } else { // vm.count("output-uri") != 0u
        role = Receiver;
    }

    return;
}

Parameters::Parameters(int argc, char** argv) {
    parse_options(argc, argv);
}
