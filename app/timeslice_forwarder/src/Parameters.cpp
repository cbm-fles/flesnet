#include <df/Utils/Utils.hpp>
#include "Parameters.hpp"
#include <boost/program_options.hpp>
#include <cstdlib>
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
    vector<string> connectors_option;
    vector<string> input_option;
    vector<string> output_option;
    vector<string> central_manager_option;

    general_add("help,h", "Print this help");
    // general_add("central-manager", )
    general_add("connector",
        po::value<vector<string>>(&connectors_option)->composing(),
        "Add a connector with '--connector \"<name> <listen_addr> <UID>\"'. Note the quotation marks surrounding the argument. To use multiple connectors simply use this flag multiple times with the shown pattern.");
    general_add("input",
        po::value<vector<string>>(&input_option)->composing(),
        "Adds an input connector with '--input \"<name> <listen_addr>\"'. Note: UID is not necessary here. Usually this is set for the node acting as the input to the network.");
    general_add("output",
        po::value<vector<string>>(&output_option)->composing(),
        "Adds an input connector with '--output \"<name> <listen_addr>\"'. Note: UID is not necessary here. Usually this is set for the node acting as the output to the network.");
    general_add("central-manager",
        po::value<vector<string>>(&central_manager_option)->composing(),
        "Adds an central manager connector with '--central-manager \"<name> <listen_addr>\"'. Note: UID is not necessary here.");
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
        << "Usage:" << endl
        << "!! TODO"
        << endl << endl
        << "Command line options";
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

    if (vm.count("group-id") != 0 || vm.count("node-id") != 0) {
        is_node = true;
        for (auto &connector_str : connectors_option) {
            vector<string> connectors_option_splitted = split(connector_str, ' ');
            if (connectors_option_splitted.size() != 3) {
                cerr << "!!! Connector definition not valid: " << connector_str << endl;
                exit(EXIT_FAILURE);
            }
            WiConnectorConfig c;
            c.type = WorkItem::connector_config;
            c.name = connectors_option_splitted[0];
            c.listen_addr = connectors_option_splitted[1];
            try {
                c.connector_uid = stoull(connectors_option_splitted[2]);
            } catch (...) {
                cerr << "connector UID has to be an integer >= 0. Given was: " << connectors_option_splitted[2] << endl;
                exit(EXIT_FAILURE);
            }
            connectors.push_back(c);
        }

        // if (shm_name.empty()) {
        //     cerr << "!!! No SHM name provided" << endl;
        //     exit(EXIT_FAILURE);
        // }
    }

    if (vm.count("input") != 0) {
        is_node = true;
        vector<string> input_option_splitted = split(input_option[0], ' ');
        // cout << "INPUT OPTION: " << input_option[0] << endl;
        if (input_option_splitted.size() < 2) {
            cout << "!!! Input option not valid" << endl;
            exit(EXIT_FAILURE);
        }

        input.type = WorkItem::connector_config;
        input.name = input_option_splitted[0];
        input.listen_addr = input_option_splitted[1];
    }


    if (vm.count("output") != 0) {
        is_node = true;
        vector<string> output_option_splitted = split(output_option[0], ' ');
        if (output_option_splitted.size() < 2) {
            cerr << "!!! Output option not valid" << endl;
            exit(EXIT_FAILURE);
        }

        output.type = WorkItem::connector_config;
        output.name = output_option_splitted[0];
        output.listen_addr = output_option_splitted[1];
    }

    if (vm.count("central-manager") != 0) {
        vector<string> central_manager_option_splitted = split(central_manager_option[0], ' ');
        central_manager.type = WorkItem::connector_config;
        central_manager.name = central_manager_option_splitted[0];
        central_manager.listen_addr = central_manager_option_splitted[1];
    }
}

Parameters::Parameters(int argc, char** argv) {
    parse_options(argc, argv);
}
