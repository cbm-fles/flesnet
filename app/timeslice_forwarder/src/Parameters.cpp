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
        "Defines interface listen address to be used by other nodes to establish a connnection '-l <listen_address>");

    general_add("central-manager,c",
        po::value<string>(&central_manager_listen_addr),
        "If this is defined by its own the node will run as the central manager. Otherwise combine it with -o or -i so the node can connect to the central manager.");

    general_add("monitor,m",
        po::value<string>(&monitoring_uri),
        "monitoring uri influxdb2:host:bucket:[auth_token]");

    general_add("node-id,n",
        po::value<uint32_t>(&node_id),
        "Sets the node ID for this node");

    general_add("group-id,g",
        po::value<uint32_t>(&group_id),
        "Sets the group ID for this node");

    general_add("shm-id",
        po::value<std::string>(&shm_name),
        "SHM name");

    stringstream desc_sstr;
    desc_sstr << endl
        << "Start Central Manager: " << endl
        << "\t" << "timeslice_forwarder --central-manager \"ConnectorInfiniband <ip>:<port>\"" << endl << endl
        << "Start Input Node:" << endl
        << "\t" << R"(timeslice_forwarder --node-id 1 --group-id 1 --central-manager "ConnectorInfiniband <ip>:<port>" --connector "ConnectorInfiniband <own_ip>:<port> 1" --input "ConnectorFromFlesnet <shm_id>")" << endl << endl
        << endl << endl;
        // << "Command line options";
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
