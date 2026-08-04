#pragma once

#include <boost/none_t.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <boost/assign/list_of.hpp>
#include <df/WorkItems/WiConnectorConfig.hpp>

class Parameters {
private:
    uint64_t wi_buffer_size_in_mb = 5;

public:
    enum Role {
        CentralManager,
        Sender,
        Receiver,
        None
    };

    Role role = Role::None;
    std::vector<WiConnectorConfig> connectors;
    uint32_t node_id = 0;
    // uint32_t group_id = 0;
    // std::string shm_uri;
    std::string listen_addr;
    std::string central_manager_listen_addr;
    std::string monitoring_uri;
    std::string output_uri;
    std::string input_uri;
    uint32_t timeslice_size = 100;
    uint64_t data_buffer_map_size = 4096;
    uint64_t wi_buffer_size{wi_buffer_size_in_mb * 1000};
    uint64_t wi_buffer_map_size = 4096;

    Parameters(int argc, char** argv);
    Parameters() = default;
    void parse_options(int argc, char** argv);

};
