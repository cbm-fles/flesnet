#include <atomic>
#include <boost/proto/proto_fwd.hpp>
#include <boost/thread/pthread/thread_data.hpp>
#include <cstddef>
#include <TimesliceWriter.hpp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <getopt.h>
#include <chrono>
#include <memory>
#include <mutex>
#include <rdma/fabric.h>
#include <sched.h>
#include <shared_mutex>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include "TimesliceReader.hpp"
#include "TsclientWriter.hpp"
#include "Tssink.hpp"
// #include <df/Scheduler/SchedulerA.hpp>
// #include <df/SchedulerInstructionDecoder/SidSchedulerA.hpp>
#include <sys/stat.h>        /* For mode constants */
#include "ConnectorFromFlesnet.hpp"
#include "Parameters.hpp"
#include <df/CentralManagers/CentralManager.hpp>
#include <df/WorkItems/WiData.hpp>
#include <df/WorkItems/WorkItem.hpp>
#include <df/EvaluationLogic/EvaluationLogic.hpp>
#include <df/WorkItems/WiConnectorConfig.hpp>
#include <df/Node.hpp>
#include "df/BufferMap/BufferMap.hpp"
#include "df/CentralManagers/CentralManagerInterface.hpp"
#include "df/Utils/CallbackContainer.hpp"
#include "df/WorkItems/WiNodeConfig.hpp"
#include "df/WorkerThread.hpp"
#include <df/Connectors/ConnectorFile.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#include <df/InterfaceFactory.hpp>
#include <df/Connectors/ConnectorInfiniband.hpp>
#include <df/WorkItems/WiTransmission.hpp>

#include <df/CentralManagers/CentralManager.hpp>
#include <df/ConnectionManager.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <iostream>
// #define DEFAULT_CONNECTOR_CLASSES ConnectorEthernetTCP, ConnectorInfinibandMsg, ConnectorFile, ConnectorInfinibandMsgRmaNew
#define DEFAULT_CM_CLASSES CentralManager

using namespace std;

Parameters par;
constexpr uint64_t BUFFER_MAP_ELEMENTS = 256;
constexpr uint64_t DATA_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 450;
constexpr uint64_t WI_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 5;



int start_cm() {
    auto node = make_shared<Node>(0, 0);
    std::shared_mutex mtx;
    std::mutex buffer_evaluation_mtx;
    unordered_map<uint64_t, std::string> uid_address_map;
    unordered_map<uint64_t, std::string> uid_listen_address_map;
    unordered_map<uint64_t, std::shared_ptr<BufferMap>> uid_buffer_map_map;
    WorkerThread worker;
    ConnectionManager connection_manager;
    // InterfaceFactory<ConnectorInterface, ConnectorFromFlesnet, ConnectorInfiniband> connector_factory;
    const auto cm_address = par.central_manager.listen_addr;

    // const auto node_connector = connector_factory.get("");
    auto node_connector = make_shared<ConnectorInfiniband>();
    const auto node_listen_addr = par.central_manager_listen_addr;
    const auto node_connector_uid = 0;

    const auto wi_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    const auto wi_buffer = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());

    const auto data_buffer = std::shared_ptr<char>(new char[DATA_BUFFER_SIZE], std::default_delete<char[]>());
    const auto data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, DATA_BUFFER_SIZE);

    node->set_wi_buffer(wi_buffer, wi_buffer_map, WI_BUFFER_SIZE);
    node->set_data_buffer(data_buffer, data_buffer_map, DATA_BUFFER_SIZE);
    node->add_connector(node_connector, node_connector_uid, node_listen_addr);

    auto eval_node_status = [&] (uint32_t group_id, uint32_t node_id) {
        const auto node_uid = MAKE_UID(group_id, node_id);
        std::unique_lock<mutex> l1(buffer_evaluation_mtx, std::defer_lock);
        std::shared_lock<std::shared_mutex> l2(mtx, std::defer_lock);
        std::lock(l1,l2);

        if (group_id == 1) { // TS sender
            auto connections = connection_manager.get_connections(node_uid);
            if (connections.empty()) {
                cout << "no connections available" << endl;
                return;
            }

            auto idx = rand() % connections.size();
            auto target_node_uid = connections[idx];
            auto target_node_id  = NODE_ID(target_node_uid);
            auto target_group_id  = GROUP_ID(target_node_uid);
            cout << "Planing to send to Node ID: " << target_node_id << " - Group ID: " << target_group_id << endl;
            auto wi_tx = make_shared<WiTransmission>();
            wi_tx->node_uid = target_node_uid;
            wi_tx->type = WorkItem::transmission;
            node->send_work_item(uid_address_map[node_uid], node_connector, wi_tx);
        } else if (group_id == 2) { // TS receiver
            cout << "evaluation of TS receiver status" << endl;
        } else {
            cerr << "! Invalid Group ID: " << group_id << '\n' <<
                    "! Nodes should either have Group ID 1 or 2" << endl;
        }
    };

    node->on_new_work_item([&mtx, &uid_listen_address_map, &connection_manager, node, node_connector, &uid_buffer_map_map, eval_node_status, &worker, &buffer_evaluation_mtx] (std::string address, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id) {
        auto node_uid = MAKE_UID(group_id, node_id);
        if (wi_type == WorkItem::connector_config) {
            WiConnectorConfig conn_config;
            conn_config.deserialize(wi_ptr);
            std::unique_lock<std::shared_mutex> l(mtx);
            uid_listen_address_map[node_uid] = conn_config.listen_addr;
            auto all_possible_connections = connection_manager.get_connections(node_uid, false);
            std::vector<uint64_t> relevant_connections;
            for (auto &remote_uid : all_possible_connections) {
                if (GROUP_ID(remote_uid) == group_id + 1 || GROUP_ID(remote_uid) == group_id - 1) {
                    auto wi_connection = make_shared<WiConnection>();
                    wi_connection->type = WorkItem::connection_req;
                    wi_connection->connector_uid = 0;
                    wi_connection->connector_address = uid_listen_address_map[remote_uid];
                    node->send_work_item(address, node_connector, wi_connection);
                }
            }
        } else if (wi_type == WorkItem::connection) {
            WiConnection wi_connection;
            wi_connection.deserialize(wi_ptr);
            auto from = MAKE_UID(wi_connection.from_group_id, wi_connection.from_node_id);
            auto to = MAKE_UID(wi_connection.to_group_id, wi_connection.to_node_id);
            std::unique_lock<std::shared_mutex> l(mtx);
            connection_manager.connect_unidirectional(from, to);
        } else if (wi_type == WorkItem::buffer_status) {
            cout << "received buffer status" << endl;
            std::unique_lock<mutex> l(buffer_evaluation_mtx, std::defer_lock);
            if (l.try_lock()) {
                node_connector->lock_and_get_buffer_map(
                    address,
                    Node::DATA_BUFFER_IDX,
                    [node_connector, address, &mtx, node_uid, &uid_buffer_map_map, eval_node_status, node_id, group_id, &worker] (std::shared_ptr<BufferMap> buffer_map_copy) {
                        {
                            shared_lock<shared_mutex> l(mtx);
                            uid_buffer_map_map[node_uid] = buffer_map_copy;
                        }
                        worker.dispatch(std::bind(eval_node_status, group_id, node_id));
                        node_connector->unlock_remote_buffer_map(address, buffer_map_copy, Node::DATA_BUFFER_IDX);
                    },
                    [] () {
                        return true;
                    }
                );
            }
        } else {
            cerr << "Received unknown WorkItem type: " << wi_type << endl;
        }
    });

    node->on_node_disconnected([&connection_manager, &uid_address_map, &mtx] (string /*address*/, uint64_t group_id, uint64_t node_id) {
        std::unique_lock<std::shared_mutex> l(mtx);
        auto node_uid = MAKE_UID(group_id, node_id);
        auto key_pos = uid_address_map.find(node_uid);
        uid_address_map.erase(key_pos);
        connection_manager.remove_node(node_uid);
    });

    node->on_node_connected([&connection_manager, &uid_address_map, &mtx, node, node_connector] (string address, uint64_t group_id, uint64_t node_id) {
        cout << "node connected: \n" <<
                "Group ID: " << group_id << '\n' <<
                "Node ID: " << node_id  << endl;
        auto node_uid = MAKE_UID(group_id, node_id);
        std::unique_lock<std::shared_mutex> l(mtx);
        uid_address_map[node_uid] = address;
        connection_manager.add_node(node_uid);
    });

    cout << "Central Manager listening on: " << node_listen_addr << endl;
    while (true) {
        this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}

int start_sender() {
    auto node_id = par.node_id;
    auto group_id = par.group_id;
    auto node = make_shared<Node>(node_id, group_id);
    std::shared_mutex mtx;
    unordered_map<uint64_t, std::string> uid_address_map;
    EvaluationLogic eval_logic;

    // InterfaceFactory<ConnectorInterface, ConnectorFromFlesnet, ConnectorInfiniband> connector_factory;
    const auto cm_address = par.central_manager_listen_addr;

    // const auto node_connector = connector_factory.get(par.connectors[0].name);
    const auto node_connector = make_shared<ConnectorInfiniband>();
    const auto node_listen_addr = par.input_listen_addr;
    cout << "Started as sender (" << node_listen_addr << ")" << endl;

    const auto node_connector_uid = 0;

    const auto wi_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    const auto wi_buffer = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());

    // the ts_reader connects to the shm, therefore it can give use the pointer to the memory containing the timeslices
    cout << "SHM: " << par.shm_name << endl;
    TimesliceReader ts_reader(par.shm_name);
    const auto buffer_size = ts_reader.get_buffer_size();
    auto data_buffer = std::shared_ptr<char>(ts_reader.get_buffer());
    auto wi = make_shared<WorkItem>();
    wi->type = WorkItem::buffer_status;

    ts_reader.on_new_timeslice([node, &cm_address, node_connector, wi] () {
        node->send_work_item(cm_address, node_connector, wi);
    });

    // using the shm ptr to initialize our data buffer and buffer map
    const auto data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, buffer_size);
    node->set_wi_buffer(wi_buffer, wi_buffer_map, WI_BUFFER_SIZE);
    node->set_data_buffer(data_buffer, data_buffer_map, buffer_size);

    node->add_connector(node_connector, node_connector_uid, node_listen_addr);
    ts_reader.set_buffer_map(data_buffer_map);

    node->on_new_work_item([&mtx, &uid_address_map, data_buffer_map, node, node_connector, data_buffer, &eval_logic, &ts_reader] (std::string /*address*/, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id) {
        cout << "received new work item" << endl;
        if (group_id == 0 && node_id == 0) { // received new work item from central manager
            if (wi_type == WorkItem::transmission) {
                WiTransmission wi_transmission;
                wi_transmission.deserialize(wi_ptr);
                auto remote_node_id = NODE_ID(wi_transmission.node_uid);
                auto remote_group_id = GROUP_ID(wi_transmission.node_uid);
                string rem_address;
                {
                    shared_lock<shared_mutex> l(mtx);
                    rem_address = uid_address_map[wi_transmission.node_uid];
                }
                cout << "Commanded to send data to Node ID: " << remote_node_id << " - Group ID: " << remote_group_id << " - address: " << rem_address << endl;
                auto *el = data_buffer_map->get_oldest_linked_list_element(nullptr, BufferMap::ListElement::IO::RX);
                if (el == nullptr) {
                    cout << "no oldest element available" << endl;
                    return;
                }
                uint64_t combined_size = 0;
                auto component_elements = data_buffer_map->get_elements_of_component(el->compontent_id, combined_size);
                std::cout << "requesting remote buffer map" << endl;
                node_connector->lock_and_get_buffer_map(
                    rem_address,
                    Node::DATA_BUFFER_IDX,
                    [node_connector, data_buffer, component_elements, &eval_logic, rem_address, data_buffer_map, &ts_reader] (shared_ptr<BufferMap> rem_buffer_map_copy) {
                        cout << "received buffer remote map " << endl;
                        auto rem_offsets_and_spaces = rem_buffer_map_copy->get_offsets_and_spaces();
                        auto dest_addresses = eval_logic.evaluate(component_elements, rem_offsets_and_spaces);
                        if (dest_addresses.empty()) {
                            cerr << "No space in remote buffer map available" << endl;
                            //! @todo handle properly
                            exit(-1);
                        }
                        vector<uint64_t> src_mem_addresses;
                        vector<uint64_t> sizes;
                        src_mem_addresses.resize(component_elements.size());
                        sizes.resize(component_elements.size());
                        for (uint64_t i = 0; i < component_elements.size(); i++) {
                            src_mem_addresses[i] = component_elements[i]->address;
                            sizes[i] = component_elements[i]->len;
                        }
                        // update rem_buffer_map_copy with the new content
                        rem_buffer_map_copy->insert(component_elements, dest_addresses, BufferMap::ListElement::RX);
                        node_connector->sendv(
                            rem_address,
                            data_buffer,
                            Node::DATA_BUFFER_IDX,
                            src_mem_addresses,
                            dest_addresses,
                            sizes,
                            [rem_address, node_connector, rem_buffer_map_copy, data_buffer_map, component_elements, &ts_reader] () {
                                // 1. send the new buffer map to remote node and unlock
                                node_connector->write_remote_buffer_map_and_unlock(rem_address, rem_buffer_map_copy,
                                    Node::DATA_BUFFER_IDX,
                                    [data_buffer_map, component_elements, &ts_reader] () {
                                        // 2. remove the sent ts from own buffermap
                                        data_buffer_map->remove_elements(component_elements);
                                        // 3. call clear_timeslice on ts_reader
                                        ts_reader.clear_last_timeslice();
                                    }
                                );
                            }
                        );
                    },
                    [] () {
                        return true;
                    }
                );
                cout << "combined_size: " << combined_size << endl;
            }
        }
    });

    node->on_node_connected([&node_listen_addr, node, node_connector, &node_id, &group_id, &cm_address, &uid_address_map, &mtx] (string address, uint64_t rem_group_id, uint64_t rem_node_id) {
        cout << "node connected: \n" <<
                "Group ID: " << rem_group_id << '\n' <<
                "Node ID: " << rem_node_id  << endl;

        if (rem_group_id == 0 && rem_node_id == 0) { // connected to central manager
            auto conn_config = make_shared<WiConnectorConfig>();
            conn_config->type = WorkItem::connector_config;
            conn_config->connector_uid = 0;
            conn_config->listen_addr = node_listen_addr;
            conn_config->name = "ConnectorInfiniband";
            node->send_work_item(address, node_connector, conn_config);
        } else { // Connected to some other node - tell the central manager about it
            auto const node_uid = MAKE_UID(rem_group_id, rem_node_id);
            auto wi_connection = make_shared<WiConnection>();
            wi_connection->type = WorkItem::connection;
            wi_connection->from_group_id = group_id;
            wi_connection->from_node_id = node_id;
            wi_connection->to_group_id = rem_group_id;
            wi_connection->to_node_id = rem_node_id;
            unique_lock<shared_mutex> l(mtx);
            uid_address_map[node_uid] = address;
            node->send_work_item(cm_address, node_connector, wi_connection);
        }
    });

    node->connect_to_node(cm_address, node_connector_uid);
    //! @todo figure out the race condition that makes this timeout necessary
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    ts_reader.set_node_connector(node_connector);
    ts_reader.start_timeslice_reading();
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(2000));
        // node->send_work_item(cm_address, node_connector, wi);
    }
    return 0;
}

int start_receiver() {
    auto node_id = par.node_id;
    auto group_id = par.group_id;
    auto node = make_shared<Node>(node_id, group_id);
    // InterfaceFactory<ConnectorInterface, ConnectorFromFlesnet, ConnectorInfiniband> connector_factory;
    const auto cm_address = par.central_manager_listen_addr;

    // const auto node_connector = connector_factory.get(par.connectors[0].name);
    const auto node_connector = make_shared<ConnectorInfiniband>();
    const auto node_listen_addr = par.output_listen_addr;
    cout << "Started as receiver (" << node_listen_addr << ")" << endl;
    const auto node_connector_uid = 0;

    const auto wi_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    const auto wi_buffer = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());

    // the ts_reader connects to the shm, therefore it can give use the pointer to the memory containing the timeslices
    // TimesliceReader ts_reader(par.input.listen_addr);
    // const auto buffer_size = ts_reader.get_buffer_size();
    // ts_reader.on_new_timeslice([node] () {
    //     node->send_status_to_cm();
    // });

    // using the shm ptr to initialize our data buffer and buffer map
    // const auto data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, buffer_size);
    // const auto data_buffer = std::shared_ptr<char>(ts_reader.get_buffer(), no_del(char));
    // TimesliceWriter ts_writer("./mytsarchive.tsa");

    // std::shared_ptr<TsSink> sinnk = nullptr;
    // if (true) {

    // }
    cout << "SHM: " << par.shm_name << endl;

    // cout << "tswriter init" << endl;
    TsclientWriter ts_writer(par.shm_name);
    // sleep(5);

    // cout << "tswriter init DONE" << endl;

    // const auto buffer_size = DATA_BUFFER_SIZE;
    // const auto data_buffer = std::shared_ptr<char>(new char[buffer_size], std::default_delete<char[]>());
    const auto data_buffer = ts_writer.get_buffer();
    const auto buffer_size = ts_writer.get_buffer_size();
    const auto data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, buffer_size);
    data_buffer_map->insert(0, 336, BufferMap::TAG_UNSET);
    node->set_wi_buffer(wi_buffer, wi_buffer_map, WI_BUFFER_SIZE);
    node->set_data_buffer(data_buffer, data_buffer_map, buffer_size);
    node->add_connector(node_connector, node_connector_uid, node_listen_addr);

    node->on_new_work_item([] (std::string /*address*/, std::shared_ptr<char> /*wi_ptr*/, WorkItem::Type /*wi_type*/, uint64_t group_id, uint64_t node_id) {
        if (group_id == 0 && node_id == 0) { // received new work item from central manager
        }
    });

    node->on_node_connected([&node_listen_addr, node, node_connector, &node_id, &group_id, &cm_address] (string address, uint64_t rem_group_id, uint64_t rem_node_id) {
        cout << "node connected: \n" <<
                "Group ID: " << rem_group_id << '\n' <<
                "Node ID: " << rem_node_id  << endl;

        if (rem_group_id == 0 && rem_node_id == 0) { // connected to central manager
            auto conn_config = make_shared<WiConnectorConfig>();
            conn_config->type = WorkItem::connector_config;
            conn_config->connector_uid = 0;
            conn_config->listen_addr = node_listen_addr;
            conn_config->name = "ConnectorInfiniband";
            node->send_work_item(address, node_connector, conn_config);
        } else { // Connected to some other node - tell the central manager about it
            auto wi_connection = make_shared<WiConnection>();
            wi_connection->type = WorkItem::connection;
            wi_connection->from_group_id = group_id;
            wi_connection->from_node_id = node_id;
            wi_connection->to_group_id = rem_group_id;
            wi_connection->to_node_id = rem_node_id;
            node->send_work_item(cm_address, node_connector, wi_connection);
        }
    });

    node->on_new_data([data_buffer_map, node_connector, &ts_writer] (std::string /*address*/, uint64_t /*group_id*/, uint64_t /*node_id*/) {
        node_connector->lock_buffer_map(data_buffer_map, [data_buffer_map, node_connector, &ts_writer] () {
            auto *el = data_buffer_map->get_oldest_linked_list_element(nullptr, BufferMap::ListElement::IO::RX);
            if (el == nullptr) {
                node_connector->unlock_buffer_map(data_buffer_map);
                return;
            }
            uint64_t component_size = 0;
            auto component = data_buffer_map->get_elements_of_component(el->compontent_id, component_size);
            cout << "component size: " << component_size << endl;
            ts_writer.write_timeslice(component);
            data_buffer_map->remove_elements(component);
            // data_buffer_map->get_elements_of_component(el->compontent_id, component_size);
            el = data_buffer_map->get_oldest_linked_list_element(nullptr, BufferMap::ListElement::IO::RX);
            if (el != nullptr) {
                cerr << "more data availables" << endl;
            }
            // data_buffer_map->remove_elements(component);
            node_connector->unlock_buffer_map(data_buffer_map);
        });
    });

    node->connect_to_node(cm_address, node_connector_uid);
    auto wi = make_shared<WorkItem>();
    wi->type = WorkItem::buffer_status;
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

    if (par.is_central_manager) {
        start_cm();
    } else if (par.is_sender) {
        start_sender();
    } else if (par.is_receiver) {
        start_receiver();
    } else { // Should never happen
        cerr << "! Was unable to determine if node is sender, receiver or central manager" << endl;
    }

    // if (!par.is_node) {
    //     start_cm();
    // } else if (par.group_id == 1){
    //     sender_node();
    // } else {
    //     receiver_node();
    // }
    cout << "Done" << endl;
    return 0;
}