#include "Parameters.hpp"

#include <cstdint>
#include <df/Utils/Utils.hpp>
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
    po::options_description advanced("Advanced options");
    auto general_add = general.add_options();
    auto advanced_add  = advanced.add_options();
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
        "Monitoring URI influx2:host:port:timeslice_forwarder_status:[auth_token]");

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

    advanced_add("data-buffer-map-size",
        po::value<uint64_t>(&data_buffer_map_size)
            ->default_value(data_buffer_map_size),
        "Sets how many data references the data buffer map can contain. One timeslice currently needs `<component cnt> * 2` references."
    );

    advanced_add("wi-buffer-size",
        po::value<uint64_t>(&wi_buffer_size_in_mb)
            ->default_value(wi_buffer_size_in_mb),
        "Sets how big the buffer containing the work items should be in MB."
    );

    advanced_add("wi-buffer-map-size",
        po::value<uint64_t>(&wi_buffer_map_size)
            ->default_value(wi_buffer_map_size),
        "Sets how many data references the buffer map for the WorkItem Buffer can contain. Each work item uses one reference."
    );

    stringstream desc_sstr;
    desc_sstr << endl
        << "NOTE: The TS Sender node expects the timeslices to be provided using a timeslice client via SHM." << endl
        << "Start Central Manager: " << endl
        << "timeslice_forwarder --central-manager <CM_IB_ip>:<CM_port>" << endl << endl
        << "Start TS Sender Node:" << endl
        << "timeslice_forwarder --node-id <n> -A <own_IB_ip>:<own_port> --central-manager <CM_IB_ip>:<CM_port> -i <SHM URI like in flesnet>" << endl << endl
        << "Start TS Receiver Node:" << endl
        << "timeslice_forwarder --node-id <n> -A <own_IB_ip>:<own_port> --central-manager <CM_IB_ip>:<CM_port> -o <SHM URI like in flesnet>"
        << endl << endl;

    stringstream minimal_example;
    minimal_example << endl
        << "This is a minmal example. The tsclient is used to provide timeslices via SHM on the sender side and it is used on the receiver side to consume received timeslices: " << endl
        << "" << endl
        << "                  ____________ CM ___________ " << endl
        << "                  |                          |" << endl
        << "tsclient->SHM->TS Sender->IB Connection->TS Receiver->SHM->tsclient" << endl
        << "" << endl
        << "The timeslice forwarder is robust enough for any arbitrary starting order of the participants. Nontheless it is recommended to do it in the following order:" << endl
        << "Node A: Central Manager (CM)" << endl
        << "Node B: TS Receiver and output tsclient" << endl
        << "Node C: TS Sender and input tsclient" << endl
        << "For the following example we assume that the timeslices have 29 components. This example uses IP addresses, alternatively use the hostname which resolves to the IB IP." << endl
        << "" << endl
        << "On Node A:" << endl
        << "Start the Central Manager: " << endl
        << "\t./timeslice_forwarder -c 10.253.31.143:8080" << endl
        << "" << endl
        << "On Node B:" << endl
        << "Start the TS Receiver in one process (increment N with each receiver):" << endl
        << "\t./timeslice_forwarder -c 10.253.31.143:8080 -A 10.253.30.67:8080 -o shm:ts_out?n=29 -N 1" << endl
        << "Start the tsclient which will take out the received timeslices in another process:" << endl
        << "\t./tsclient -i shm:ts_out -o your_output_archive.tsa" << endl
        << "" << endl
        << "On Node C:" << endl
        << "Start the TS Sender in one process (increment N with each sender):" << endl
        << "\t./timeslice_forwarder -c 10.253.31.143:8080 -A 10.253.31.135:8080 -i shm:ts_in -N 1" << endl
        << "Start the tsclient which will provide timeslices to the TS Sender via SHM:" << endl
        << "\t./tsclient -i your_input_archive.tsa -o shm:/ts_in?n=29 -l 0" << endl;

    po::variables_map vm;
    po::options_description desc(desc_sstr.str());
    desc
        .add(general)
        .add(advanced);

    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help") != 0) {
        cout << desc << endl;
        cout << minimal_example.str() << endl;
        exit(EXIT_SUCCESS);
    }

    wi_buffer_size_in_mb = wi_buffer_size_in_mb*1000;
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
