#pragma once

#include <boost/program_options.hpp>
#include <cstdlib>
#include <boost/assign/list_of.hpp>
#include <df/WorkItems/WiConnectorConfig.hpp>

class Parameters {
public:
    std::vector<WiConnectorConfig> connectors;
    uint32_t node_id = 0;
    uint32_t group_id = 0;
    std::string shm_uri;
    std::string listen_addr;
    std::string central_manager_listen_addr;
    std::string monitoring_uri;

    Parameters(int argc, char** argv);
    Parameters() = default;
    void parse_options(int argc, char** argv);
};
