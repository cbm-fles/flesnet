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

using namespace std;
namespace po = boost::program_options;
void Parameters::parse_options(int argc, char** argv) {
    po::options_description general("General options");
    auto general_add = general.add_options();
    string connectors_option;

    general_add("help,h", "Print this help");

    general_add("listen-address,l",
        po::value<string>(&listen_addr),
        "Defines interface listen address to be used by other nodes to establish a connnection '-l <own_IB_ip>:<port>'");

    general_add("central-manager,c",
        po::value<string>(&central_manager_listen_addr),
        "IP and port of the IB interface of the CM manager '-c <CM_IB_ip>:<port>'");

    general_add("monitor,m",
        po::value<string>(&monitoring_uri),
        "monitoring uri influxdb2:host:bucket:[auth_token]");

    general_add("node-id,n",
        po::value<uint32_t>(&node_id),
        "Sets the node ID for this node. Within one Group the node ID has to be unique.");

    general_add("group-id,g",
        po::value<uint32_t>(&group_id),
        "Sets the group ID for this node.\nGroup ID 1 = TS sender.\nGroup ID 2 = TS Receiver.");

    general_add("shm-id",
        po::value<std::string>(&shm_name),
        "SHM name with query parameters similar to tsclient and flesnet. eg. '--shm-id ts_in?n=16'");

    stringstream desc_sstr;
    desc_sstr << endl
        << "Start Central Manager: " << endl
        << "timeslice_forwarder --central-manager <CM_IB_ip>:<CM_port>" << endl << endl
        << "Start TS Sender Node:" << endl
        << "timeslice_forwarder --node-id <n> --group-id 1 -l <own_IB_ip>:<own_port> --central-manager <CM_IB_ip>:<CM_port> --shm-id <shm uri like in flesnet>" << endl << endl
        << "Start TS Receiver Node:" << endl
        << "timeslice_forwarder --node-id <n> --group-id 2 -l <own_IB_ip>:<own_port> --central-manager <CM_IB_ip>:<CM_port> --shm-id <shm uri like in flesnet>"
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

    is_monitoring_enabled = !monitoring_uri.empty(); // convinience variable
    return;
}

Parameters::Parameters(int argc, char** argv) {
    parse_options(argc, argv);
}
