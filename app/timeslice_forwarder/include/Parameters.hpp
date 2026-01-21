#pragma once

#include <boost/program_options.hpp>
#include <cstdlib>
#include <boost/assign/list_of.hpp>
#include <df/WorkItems/WiConnectorConfig.hpp>

class Parameters {
public:
    std::vector<WiConnectorConfig> connectors;
    WiConnectorConfig input;
    WiConnectorConfig output;
    WiConnectorConfig central_manager;
    uint32_t node_id = 0;
    uint32_t group_id = 0;
    bool is_node = false;
    std::string shm_name;
    std::string ib_address;
    bool is_sender = false;
    bool is_receiver = false;
    bool is_central_manager = false;
    std::string listen_addr;
    std::string central_manager_listen_addr;
    std::string input_listen_addr;
    std::string output_listen_addr;
    Parameters(int argc, char** argv);
    Parameters() = default;
    void parse_options(int argc, char** argv);
};
