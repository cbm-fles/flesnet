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

    Parameters(int argc, char** argv);
    Parameters() = default;
    void parse_options(int argc, char** argv);
};
