#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <getopt.h>
#include <chrono>
#include <memory>
#include <mutex>
#include <rdma/fabric.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>
// #include <df/Scheduler/SchedulerA.hpp>
// #include <df/SchedulerInstructionDecoder/SidSchedulerA.hpp>
#include <sys/stat.h>        /* For mode constants */
#include "ConnectorFromFlesnet.hpp"
#include "Parameters.hpp"
#include <df/CentralManagers/CentralManager.hpp>
#include <df/WorkItems/WiData.hpp>
#include <df/WorkItems/WorkItem.hpp>
#include <df/EvaluationLogic/EvaluationLogic.hpp>
// #include <df/
#include <df/WorkItems/WiConnectorConfig.hpp>
#include <df/Node.hpp>
#include "df/BufferMap/BufferMap.hpp"
#include "df/CentralManagers/CentralManagerInterface.hpp"
#include <df/Connectors/ConnectorFile.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#include <df/InterfaceFactory.hpp>
#include <df/Connectors/ConnectorInfiniband.hpp>

#include <iostream>
// #define DEFAULT_CONNECTOR_CLASSES ConnectorEthernetTCP, ConnectorInfinibandMsg, ConnectorFile, ConnectorInfinibandMsgRmaNew
#define DEFAULT_CM_CLASSES CentralManager

using namespace std;

Parameters par;
constexpr uint64_t BUFFER_MAP_ELEMENTS = 256;
constexpr uint64_t BUFFER_SIZE = 1024 * 1024 * 10;

int start_cm() {
    const auto cm_listen_addr = par.central_manager.listen_addr;

    auto buffer = shared_ptr<char>(new char[BUFFER_SIZE], std::default_delete<char[]>());
    auto buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, BUFFER_SIZE);

    InterfaceFactory<ConnectorInterface, ConnectorInfiniband> connector_factory;
    auto connector_infiniband = connector_factory.get(par.central_manager.name);

    string sender_addr;
    string receiver_addr;

    connector_infiniband->add_new_remote_buffer_callback([connector_infiniband, buffer, buffer_map] (const std::string& address, uint64_t index) {
        cout << "New remote buffer on " << address << " at index: " << index << endl;
    });

    EvaluationLogic eval_logic;
    connector_infiniband->add_buffer_map_copy_update_callback([connector_infiniband, &sender_addr, &receiver_addr, &eval_logic] (const std::string& address, std::shared_ptr<BufferMap> first_buffer_map, uint64_t index) {
        cout << "Buffer map on " << address << ", buffer index: " << index << " was updated" << endl;
        if (address != sender_addr) {
            return;
        }
        cout << "Fetching buffer map from receiver ..." << endl;
        connector_infiniband->lock_and_get_buffer_map(receiver_addr, 0, [first_buffer_map, &eval_logic] (const std::string&, std::shared_ptr<BufferMap> second_buffer_map) {
            first_buffer_map->lock();

            auto *oldes_el = first_buffer_map->get_oldest_linked_list_element();
            auto offsets_and_spaces = second_buffer_map->get_offsets_and_spaces();

            uint64_t target_address;
            eval_logic.evaluate(oldes_el->address, oldes_el->len, offsets_and_spaces, target_address);
            cout << "Caluclated target address: " << target_address << endl;

            //! @todo Transmit calculated address to


            first_buffer_map->unlock();
        });
    });

    connector_infiniband->add_connection_established_cb([&sender_addr, &receiver_addr] (std::string address, std::shared_ptr<char> /*custom_data*/, uint64_t custom_data_size) {
        if (sender_addr.empty()) {
            sender_addr = address;
        }
        cout << "Connection established to: " << address << endl;

        if (custom_data_size > 0) {
            sender_addr = address;
        } else {
            receiver_addr = address;
        }
    });
    connector_infiniband->listen_for_clients(cm_listen_addr);
    cout << "CM listening on: " << connector_infiniband->get_listen_address() << endl;
    while (true) {
        this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}

int sender_node() {
    InterfaceFactory<ConnectorInterface, ConnectorFromFlesnet, ConnectorInfiniband> connector_factory;

    const auto cm_address = par.central_manager.listen_addr;

    const auto cm_connector = connector_factory.get("ConnectorInfiniband");
    constexpr auto CM_BUFFER_SIZE = 1024 * 1024 * 8;
    const auto cm_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, CM_BUFFER_SIZE);
    const auto cm_buffer = std::shared_ptr<char>(new char[CM_BUFFER_SIZE], std::default_delete<char[]>());

    const auto node_connector = connector_factory.get("ConnectorInfiniband");
    constexpr auto BUFFER_SIZE = 1024 * 1024 * 450; ///< @todo calculate proper BUFFER_SIZE depending on flesnet output
    const auto buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, BUFFER_SIZE);
    const auto buffer = std::shared_ptr<char>(new char[BUFFER_SIZE], std::default_delete<char[]>());

    atomic_uint16_t link_cnt = 0;
    cm_connector->add_connection_established_cb([cm_connector, &cm_address, buffer, buffer_map, cm_buffer, cm_buffer_map, &link_cnt] (std::string, std::shared_ptr<char>, uint64_t) {
        cout << "CM connection established" << endl;
        cm_connector->link_rdma_buffer(cm_address, buffer, BUFFER_SIZE, buffer_map, 0, [&link_cnt] (const std::string& address, const uint64_t& idx) {
            cout << "Buffer with index " << idx << " was communicated to CM (" << address << ")" << endl;
            link_cnt++;
        });

        cm_connector->link_rdma_buffer(cm_address, cm_buffer, CM_BUFFER_SIZE, cm_buffer_map, 1, [&link_cnt] (const std::string& address, const uint64_t& idx) {
            cout << "Buffer with index " << idx << " was communicated to CM (" << address << ")" << endl;
            link_cnt++;
        });
    });

    std::shared_ptr<char> dummy_data = std::shared_ptr<char>(new char[1], std::default_delete<char[]>());
    cm_connector->connect_to_server(cm_address, dummy_data, 1);

    while (link_cnt != 2) {
        cout << "Waiting for buffers to get linked ..." << endl;
        this_thread::sleep_for(chrono::milliseconds(500));
    }

    buffer_map->add_elements_inserted_callback([&cm_address, buffer_map, cm_connector] () {
        cout << "BufferMap insert callback" << endl;
        cm_connector->lock_and_update_remote_buffer_map_copy(cm_address, buffer_map, 0);
    });

    *reinterpret_cast<uint64_t*>(buffer.get()) = 4211;
    buffer_map->insert(0, sizeof(uint64_t), 99999, BufferMap::ListElement::RX);

    while (true) {
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
}


int receiver_node() {
    InterfaceFactory<ConnectorInterface, ConnectorFromFlesnet, ConnectorInfiniband> connector_factory;

    const auto cm_address = par.central_manager.listen_addr;

    const auto cm_connector = connector_factory.get("ConnectorInfiniband");
    constexpr auto CM_BUFFER_SIZE = 1024 * 1024 * 8;
    const auto cm_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, CM_BUFFER_SIZE);
    const auto cm_buffer = std::shared_ptr<char>(new char[CM_BUFFER_SIZE], std::default_delete<char[]>());

    const auto node_connector = connector_factory.get("ConnectorInfiniband");
    constexpr auto BUFFER_SIZE = 1024 * 1024 * 450; ///< @todo calculate proper BUFFER_SIZE depending on flesnet output
    const auto buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, BUFFER_SIZE);
    const auto buffer = std::shared_ptr<char>(new char[BUFFER_SIZE], std::default_delete<char[]>());

    atomic_uint16_t link_cnt = 0;
    cm_connector->add_connection_established_cb([cm_connector, &cm_address, buffer, buffer_map, cm_buffer, cm_buffer_map, &link_cnt] (std::string, std::shared_ptr<char>, uint64_t) {
        cout << "CM connection established" << endl;
        cm_connector->link_rdma_buffer(cm_address, buffer, BUFFER_SIZE, buffer_map, 0, [&link_cnt] (const std::string& address, const uint64_t& idx) {
            cout << "Buffer with index " << idx << " was communicated to CM (" << address << ")" << endl;
            link_cnt++;
        });

        cm_connector->link_rdma_buffer(cm_address, cm_buffer, CM_BUFFER_SIZE, cm_buffer_map, 1, [&link_cnt] (const std::string& address, const uint64_t& idx) {
            cout << "Buffer with index " << idx << " was communicated to CM (" << address << ")" << endl;
            link_cnt++;
        });
    });
    buffer_map->insert(0, 12);
    cm_connector->connect_to_server(cm_address);

    while (link_cnt != 2) {
        cout << "Waiting for buffers to get linked ..." << endl;
        this_thread::sleep_for(chrono::milliseconds(500));
    }


    while (true) {
        this_thread::sleep_for(chrono::milliseconds(3000));
    }


    return 0;
}





int main (int argc, char** argv) {
    par.parse_options(argc, argv);
    if (FI_VERSION(FI_MAJOR_VERSION, FI_MINOR_VERSION) != fi_version()) {
        cerr << "Libfabric: Header version and library version do not match" << endl;
        cerr << "Header: " << FI_MAJOR_VERSION << "." << FI_MINOR_VERSION << endl;
        cerr << "Lib: " << FI_MAJOR(fi_version()) << "." << FI_MINOR(fi_version()) << endl;
        exit(-1);
    } else if (FI_MAJOR_VERSION != 2 || FI_MINOR_VERSION < 1) {
        cerr << "Libfabric: invalid version." << endl;
        cerr << "Minimum version required 2.1 - found " << FI_MAJOR_VERSION << "." << FI_MINOR_VERSION << endl;
    }

    if (!par.is_node) {
        start_cm();
    } else if (par.group_id == 1){
        sender_node();
    } else {
        receiver_node();
    }

    return 0;
}