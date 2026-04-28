#include <atomic>
#include <boost/proto/proto_fwd.hpp>
#include <boost/thread/pthread/thread_data.hpp>
#include <TimesliceWriter.hpp>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <future>
#include <getopt.h>
#include <chrono>
#include <memory>
#include <mutex>
#include <rdma/fabric.h>
#include <sched.h>
#include <shared_mutex>
#include <string>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include "Metric.hpp"
#include "Monitor.hpp"
#include "TimesliceReader.hpp"
#include "TsclientWriter.hpp"
#include "Tssink.hpp"
#include <sys/stat.h>        /* For mode constants */
#include "Parameters.hpp"
#include <df/WorkItems/WiData.hpp>
#include <df/WorkItems/WorkItem.hpp>
#include <df/EvaluationLogic/EvaluationLogic.hpp>
#include <df/WorkItems/WiConnectorConfig.hpp>
#include <df/Node.hpp>
#include <df/BufferMap/BufferMap.hpp>
#include <df/WorkerThread.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#include <df/InterfaceFactory.hpp>
#include <df/Connectors/ConnectorInfiniband.hpp>
#include <df/WorkItems/WiTransmission.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#include <df/ConnectionManager.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <iostream>
#include <MonitorSinkInflux2.hpp>
using namespace std;

Parameters par;
constexpr uint64_t BUFFER_MAP_ELEMENTS = 512;
constexpr uint64_t DATA_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 450;
constexpr uint64_t WI_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 5;



int start_cm() {
    auto monitor = make_shared<cbm::Monitor>();
    if (par.is_monitoring_enabled) {
        monitor->OpenSink(par.monitoring_uri);
    }

    auto node = make_shared<Node>(0, 0);
    mutex mtx;

    // maps used to translate node UID to IP addresses
    unordered_map<uint64_t, std::string> uid_address_map;
    unordered_map<uint64_t, std::string> uid_listen_address_map;

    // used to translate between node uid and its WI buffer map
    unordered_map<uint64_t, std::shared_ptr<BufferMap>> uid_buffer_map_map;

    WorkerThread worker;
    ConnectionManager connection_manager; // used to take track of connections between nodes
    const auto cm_address = par.central_manager_listen_addr;

    // const auto node_connector = connector_factory.get("");
    std::shared_ptr<ConnectorInterface> node_connector = make_shared<ConnectorInfiniband>();
    const auto node_listen_addr = par.central_manager_listen_addr;

    const auto wi_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    const auto wi_buffer = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());

    const auto data_buffer = std::shared_ptr<char>(new char[DATA_BUFFER_SIZE], std::default_delete<char[]>());
    const auto data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, DATA_BUFFER_SIZE);

    node->set_wi_buffer(wi_buffer, wi_buffer_map, WI_BUFFER_SIZE);
    node->set_data_buffer(data_buffer, data_buffer_map, DATA_BUFFER_SIZE);
    node->add_connector(node_connector, node_listen_addr);

    // This lambda callback will be called to evaluate data transmission based on the status update of the given node
    atomic_uint16_t target_idx = 0;
    auto eval_node_status = [&] (uint32_t group_id, uint32_t node_id) {
        const auto node_uid = MAKE_UID(group_id, node_id);
        unique_lock<mutex> l1(mtx);

        if (group_id == 1) { // TS sender
            auto connections = connection_manager.get_connections(node_uid);
            if (connections.empty()) {
                cout << "no connections available" << endl;
                return;
            }
            target_idx++;
            auto target_node_uid = connections[target_idx % connections.size()];
            auto target_node_id  = NODE_ID(target_node_uid);
            auto target_group_id  = GROUP_ID(target_node_uid);
            cout << "Planing to send data from N:" << node_id << " - G: " << group_id  << " to N: " << target_node_id << " - G: " << target_group_id << endl;
            auto wi_tx = make_shared<WiTransmission>();
            wi_tx->node_uid = target_node_uid;
            wi_tx->type = WorkItem::transmission;
            node->send_work_item(uid_address_map[node_uid], wi_tx);
        } else if (group_id == 2) { // TS receiver
            cout << "evaluation of TS receiver status" << endl;
        } else {
            cerr << "! Invalid Group ID: " << group_id << '\n' <<
                    "! Nodes should either have Group ID 1 or 2" << endl;
        }
    };

    atomic_bool stop_eval_worker = false;
    condition_variable eval_worker_cv;
    auto nodes_with_buffer_change = make_shared<vector<uint64_t>>();

    auto fetch_buffer_maps = [&] (const vector<uint64_t>& node_uids) {
        atomic_uint64_t cnt = node_uids.size();
        for (auto const& uid : node_uids) {
            unique_lock<mutex> l(mtx);
            string address = uid_address_map[uid];
            node_connector->lock_and_get_buffer_map(
                address,
                Node::DATA_BUFFER_IDX,
                [&, address] (std::shared_ptr<BufferMap> buffer_map_copy) {
                    node_connector->unlock_remote_buffer_map(address, buffer_map_copy, Node::DATA_BUFFER_IDX);
                    unique_lock<mutex> l(mtx);
                    if (uid_buffer_map_map.find(uid) == uid_buffer_map_map.end()) {
                        uid_buffer_map_map[uid] = buffer_map_copy;
                    }
                    cnt--;
                },
                [] () {
                    return true;
                }
            );
        }
        while (cnt != 0) {};
    };

    future<void> eval_worker = async(launch::async, [&] {
        std::vector<uint64_t> nodes_cpy;
        while (!stop_eval_worker) {
            {
                unique_lock<mutex> l(mtx);
                eval_worker_cv.wait(l, [&] () {
                    return !nodes_with_buffer_change->empty() || stop_eval_worker;
                });
                nodes_cpy = *nodes_with_buffer_change;
                nodes_with_buffer_change->clear();
            }
            // fetch_buffer_maps(nodes_cpy);
            for (auto const& uid : nodes_cpy) {
                eval_node_status(GROUP_ID(uid), NODE_ID(uid));
            }
        }
    });

    node->on_new_work_item([&mtx, &uid_listen_address_map, &connection_manager, node, node_connector, &nodes_with_buffer_change, &eval_worker_cv] (std::string address, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id) {
        auto node_uid = MAKE_UID(group_id, node_id);
        cout << "new work item: " << node_id << " - group_id " << group_id << endl;

        if (wi_type == WorkItem::connector_config) { // The given node informed us about its connection possibilities
            cout << "-- connector config" << endl;
            WiConnectorConfig conn_config;
            conn_config.deserialize(wi_ptr);
            unique_lock<mutex> l(mtx);
            uid_listen_address_map[node_uid] = conn_config.listen_addr;
            auto all_possible_connections = connection_manager.get_connections(node_uid, false);
            // vector<uint64_t> relevant_connections;
            // cout << "relevant connections: " << relevant_connections.size() << endl;
            for (auto &remote_uid : all_possible_connections) {
                if (GROUP_ID(remote_uid) == group_id + 1 || GROUP_ID(remote_uid) == group_id - 1) {
                    std::cout << "tell N: " << node_id << " - G: " << group_id << " ---> N: "<< NODE_ID(remote_uid) << " - G: " << GROUP_ID(remote_uid) << std::endl;
                    auto wi_connection = make_shared<WiConnection>();
                    wi_connection->type = WorkItem::connection_req;
                    wi_connection->connector_uid = 0;
                    wi_connection->connector_address = uid_listen_address_map[remote_uid];
                    node->send_work_item(address, wi_connection);
                }
            }
        } else if (wi_type == WorkItem::connection) { // The given node informed us about a new available connection
            cout << "-- connection" << endl;
            WiConnection wi_connection;
            wi_connection.deserialize(wi_ptr);
            auto from = MAKE_UID(wi_connection.from_group_id, wi_connection.from_node_id);
            auto to = MAKE_UID(wi_connection.to_group_id, wi_connection.to_node_id);
            unique_lock<mutex> l(mtx);
            connection_manager.connect_unidirectional(from, to);
        } else if (wi_type == WorkItem::buffer_status) { // The told us, that its buffer map has changed
            cout << "-- buffer status" << endl;
            unique_lock<mutex> l(mtx);
            nodes_with_buffer_change->push_back(node_uid);
            eval_worker_cv.notify_all();
        } else {
            cerr << "Received unknown WorkItem type: " << wi_type << endl;
        }
    });

    atomic_uint64_t input_nodes_cnt = 0;
    atomic_uint64_t output_nodes_cnt = 0;

    node->on_node_disconnected([&connection_manager, &uid_address_map, &mtx, &input_nodes_cnt, &output_nodes_cnt, monitor] (string /*address*/, uint64_t group_id, uint64_t node_id) {
        unique_lock<mutex> l(mtx);
        auto node_uid = MAKE_UID(group_id, node_id);
        auto key_pos = uid_address_map.find(node_uid);
        uid_address_map.erase(key_pos);
        connection_manager.remove_node(node_uid);
        if (group_id == 1) {
            monitor->QueueMetric("timeslice_forwarder_state",
                {{"CM", "CM"}},
                {{"input_nodes_cnt", --input_nodes_cnt}});
        } else {
            monitor->QueueMetric("timeslice_forwarder_state",
                {{"CM", "CM"}},
                {{"output_nodes_cnt", --output_nodes_cnt}});
        }
    });

    node->on_node_connected([&connection_manager, &uid_address_map, &mtx, node, node_connector, &input_nodes_cnt, &output_nodes_cnt, monitor] (string address, uint64_t group_id, uint64_t node_id) {
        cout << "node connected: \n" <<
                "Node ID: " << node_id  << '\n' <<
                "Group ID: " << group_id << std::endl;

        auto node_uid = MAKE_UID(group_id, node_id);
        if (group_id == 1) {
            monitor->QueueMetric("timeslice_forwarder_state",
                {{"CM", "CM"}},
                {{"input_nodes_cnt", ++input_nodes_cnt}});
        } else {
            monitor->QueueMetric("timeslice_forwarder_state",
                {{"CM", "CM"}},
                {{"output_nodes_cnt", ++output_nodes_cnt}});
        }
        unique_lock<mutex> l(mtx);
        uid_address_map[node_uid] = address;
        connection_manager.add_node(node_uid);

    });
    node->start();
    cout << "Central Manager listening on: " << node_listen_addr << endl;
    while (true) {
        this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}

int start_sender() {
    auto monitor = make_shared<cbm::Monitor>();
    if (par.is_monitoring_enabled) {
        monitor->OpenSink(par.monitoring_uri);
    }

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
    const auto node_listen_addr = par.listen_addr;
    cout << "Started as sender (" << node_listen_addr << ")" << endl;

    const auto node_connector_uid = 0;

    const auto wi_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    const auto wi_buffer = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());

    // the ts_reader connects to the shm, therefore it can give use the pointer to the memory containing the timeslices
    cout << "SHM: " << par.shm_name << endl;
    TimesliceReader ts_reader(par.shm_name);
    const auto buffer_size = ts_reader.get_buffer_size();
    auto data_buffer = std::shared_ptr<char>(ts_reader.get_buffer());
    atomic_bool is_cm_connected = false;

    auto wi = make_shared<WorkItem>();
    wi->type = WorkItem::buffer_status;

    ts_reader.on_new_timeslice([node, &cm_address, node_connector, wi] () {
        node->send_work_item(cm_address, wi);
    });

    // using the shm ptr to initialize our data buffer and buffer map
    const auto data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, buffer_size);
    node->set_wi_buffer(wi_buffer, wi_buffer_map, WI_BUFFER_SIZE);
    node->set_data_buffer(data_buffer, data_buffer_map, buffer_size);

    node->add_connector(node_connector, node_listen_addr);
    ts_reader.set_buffer_map(data_buffer_map);
    std::atomic_uint64_t send_cnt = 0;
    std::atomic_uint64_t failed_remote_lock = 0;

    node->on_new_work_item([&failed_remote_lock, &mtx, &uid_address_map, data_buffer_map, node, node_connector, data_buffer, &eval_logic, &ts_reader, &send_cnt, monitor] (std::string /*address*/, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id) {
        if (group_id == 0 && node_id == 0) { // received new work item from central manager
            if (wi_type == WorkItem::transmission) { // CM told us to send data to a specific node
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
                if (el == nullptr) { // no oldest element available
                    return;
                }

                uint64_t combined_size = 0;
                auto component_elements = data_buffer_map->get_elements_of_component(el->compontent_id, combined_size);
                auto *data_write_chain = new std::function<void()>;
                (*data_write_chain) = [data_write_chain, node_connector, rem_address, data_buffer, component_elements, &eval_logic, data_buffer_map, &ts_reader, &send_cnt, monitor, combined_size, &failed_remote_lock] () {
                    node_connector->lock_and_get_buffer_map(
                        rem_address,
                        Node::DATA_BUFFER_IDX,
                        [node_connector, data_write_chain, data_buffer, component_elements, &eval_logic, rem_address, data_buffer_map, &ts_reader, &send_cnt, monitor, combined_size] (shared_ptr<BufferMap> rem_buffer_map_copy) {
                            auto rem_offsets_and_spaces = rem_buffer_map_copy->get_offsets_and_spaces();
                            auto dest_addresses = eval_logic.evaluate(component_elements, rem_offsets_and_spaces);
                            if (dest_addresses.empty()) {
                                node_connector->unlock_remote_buffer_map(
                                    rem_address,
                                    rem_buffer_map_copy,
                                    Node::DATA_BUFFER_IDX,
                                    [data_write_chain] () {
                                        std::this_thread::sleep_for(std::chrono::nanoseconds(500));
                                        (*data_write_chain)();
                                    }
                                );
                            }
                            vector<uint64_t> src_mem_addresses;
                            vector<uint64_t> sizes;
                            src_mem_addresses.resize(component_elements.size());
                            sizes.resize(component_elements.size());
                            for (uint64_t i = 0; i < component_elements.size(); i++) {
                                src_mem_addresses[i] = component_elements[i]->address;
                                sizes[i] = component_elements[i]->len;
                            }
                            bool insert_successfull = rem_buffer_map_copy->insert(component_elements, dest_addresses, BufferMap::ListElement::RX);
                            // cout << "Buffer Map after: " << send_cnt << endl;
                            // rem_buffer_map_copy->print_all();


                            if (!insert_successfull) {
                                node_connector->unlock_remote_buffer_map(
                                    rem_address,
                                    rem_buffer_map_copy,
                                    Node::DATA_BUFFER_IDX,
                                    [data_write_chain] () {
                                        std::this_thread::sleep_for(std::chrono::nanoseconds(500));
                                        (*data_write_chain)();
                                    }
                                );
                                return;
                            }
                            delete data_write_chain;

                            node_connector->sendv(
                                rem_address,
                                data_buffer,
                                Node::DATA_BUFFER_IDX,
                                src_mem_addresses,
                                dest_addresses,
                                sizes,
                                [rem_address, node_connector, rem_buffer_map_copy, data_buffer_map, component_elements, &ts_reader, &send_cnt, monitor, combined_size] () {
                                    // send the new buffer map to remote node and unlock
                                    node_connector->write_remote_buffer_map_and_unlock(rem_address, rem_buffer_map_copy,
                                        Node::DATA_BUFFER_IDX,
                                        [data_buffer_map, component_elements, &ts_reader, &send_cnt, monitor, combined_size] () {
                                            monitor->QueueMetric("timeslice_forwarder_state",
                                                {{"host", std::to_string(par.node_id) + " - " + std::to_string(par.group_id)}},
                                                {{"send_cnt", ++send_cnt}});
                                            monitor->QueueMetric("timeslice_forwarder_state",
                                                {{"host", std::to_string(par.node_id) + " - " + std::to_string(par.group_id)}},
                                                {{"bytes_sent", combined_size}});
                                            // remove the sent TS from own buffermap
                                            data_buffer_map->remove_elements(component_elements);
                                            // call clear_timeslice on ts_reader
                                            ts_reader.clear_last_timeslice();
                                        }
                                    );
                                }
                            );
                        },
                        [&failed_remote_lock, monitor] () {
                            monitor->QueueMetric("timeslice_forwarder_state",
                                {{"host", std::to_string(par.node_id) + " - " + std::to_string(par.group_id)}},
                                {{"failed_remote_lock", ++failed_remote_lock}});
                            return true;
                        }
                    );
                };
                (*data_write_chain)();
            }
        }
    });
    node->on_node_connected([&node_listen_addr, node, node_connector, &node_id, &group_id, &cm_address, &uid_address_map, &mtx, &is_cm_connected, wi] (string address, uint64_t rem_group_id, uint64_t rem_node_id) {
        cout << "Node connected: \n" <<
                "Group ID: " << rem_group_id << '\n' <<
                "Node ID: " << rem_node_id  << endl;

        if (rem_group_id == 0 && rem_node_id == 0) { // connected to central manager - tell it about our connection possibilities
            auto conn_config = make_shared<WiConnectorConfig>();
            conn_config->type = WorkItem::connector_config;
            conn_config->connector_uid = 0;
            conn_config->listen_addr = node_listen_addr;
            conn_config->name = "ConnectorInfiniband";
            node->send_work_item(address, conn_config);
            is_cm_connected = true;
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
            node->send_work_item(cm_address, wi_connection, [wi, cm_address, node_connector, node] () {
                node->send_work_item(cm_address, wi);
            });
        }
    });

    node->on_connection_refused([node, cm_address] (std::string address) {
        if (address == cm_address) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            node->connect_to_node(cm_address);
        }
    });
    node->start();
    node->connect_to_node(cm_address, node_connector_uid);
    while (!is_cm_connected) {};
    //! @todo figure out the race condition that makes this timeout necessary
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    ts_reader.set_node_connector(node_connector);
    ts_reader.start_timeslice_reading();
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(2000));
    }
    return 0;
}

int start_receiver() {
    auto monitor = make_shared<cbm::Monitor>();
    if (par.is_monitoring_enabled) {
        monitor->OpenSink(par.monitoring_uri);
    }

    auto node_id = par.node_id;
    auto group_id = par.group_id;
    auto node = make_shared<Node>(node_id, group_id);
    const auto cm_address = par.central_manager_listen_addr;

    // const auto node_connector = connector_factory.get(par.connectors[0].name);
    const auto node_connector = make_shared<ConnectorInfiniband>();

    const auto node_listen_addr = par.listen_addr;
    cout << "Started as receiver (" << node_listen_addr << ")" << endl;

    const auto wi_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    const auto wi_buffer = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());

    // cout << "tswriter init" << endl;
    shared_ptr<TsSink> ts_sink = nullptr;
    std::shared_ptr<char> data_buffer = nullptr;
    std::shared_ptr<char> data_buffer_size = nullptr;
    uint64_t buffer_size = 0;
    std::function<void(std::string /*address*/, uint64_t /*group_id*/, uint64_t /*node_id*/)> on_new_work_item;
    std::shared_ptr<BufferMap> data_buffer_map = nullptr;

    if (true) { // Hardcoded switch for manual debugging
        ts_sink = make_shared<TsclientWriter>(par.shm_name);
        data_buffer = ts_sink->get_buffer();
        buffer_size = ts_sink->get_buffer_size();
    } else {
        ts_sink = make_shared<TimesliceWriter>("tsout.tsa");
        buffer_size = DATA_BUFFER_SIZE;
        data_buffer = std::shared_ptr<char>(new char[buffer_size], std::default_delete<char[]>());
        ts_sink->set_buffer(data_buffer);
    }

    data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, buffer_size);
    /**
     * The shared memory is represented by boost::managed_shared_memory.
     * Boost seems to store some metadata for its management in the SHM too.
     * It seems to be constant 336 byte. Therefore this needs to be represented in the buffer map too.
     */
    data_buffer_map->insert(0, 336, BufferMap::TAG_UNSET);
    node->set_wi_buffer(wi_buffer, wi_buffer_map, WI_BUFFER_SIZE);
    node->set_data_buffer(data_buffer, data_buffer_map, buffer_size);
    node->add_connector(node_connector, node_listen_addr);

    // Currently unused
    node->on_new_work_item([] (std::string /*address*/, std::shared_ptr<char> /*wi_ptr*/, WorkItem::Type /*wi_type*/, uint64_t group_id, uint64_t node_id) {
        if (group_id == 0 && node_id == 0) { // received new work item from central manager

        }
    });

    node->on_node_connected([&node_listen_addr, node, node_connector, &node_id, &group_id, &cm_address] (string address, uint64_t rem_group_id, uint64_t rem_node_id) {
        cout << "node connected: \n" <<
                "Group ID: " << rem_group_id << '\n' <<
                "Node ID: " << rem_node_id  << endl;

        if (rem_group_id == 0 && rem_node_id == 0) { // connected to central manager - tell it about our connection possibilities
            auto conn_config = make_shared<WiConnectorConfig>();
            conn_config->type = WorkItem::connector_config;
            conn_config->connector_uid = 0;
            conn_config->listen_addr = node_listen_addr;
            conn_config->name = "ConnectorInfiniband";
            node->send_work_item(address, conn_config);
        } else { // Connected to some other node - tell the central manager about it
            auto wi_connection = make_shared<WiConnection>();
            wi_connection->type = WorkItem::connection;
            wi_connection->from_group_id = group_id;
            wi_connection->from_node_id = node_id;
            wi_connection->to_group_id = rem_group_id;
            wi_connection->to_node_id = rem_node_id;
            node->send_work_item(cm_address, wi_connection, [] () {
                cout << "send work item done (WorkItem::connection)" << endl;
            });
        }
    });

    node->on_connection_refused([node, cm_address] (std::string address) {
        cout << "connection refused" << endl;
        if (address == cm_address) {
            node->connect_to_node(cm_address);
        }
    });
    atomic_uint64_t failed_self_locks = 0;
    atomic_uint64_t recv_cnt  = 0;
    node->on_new_data([data_buffer_map, node_connector, ts_sink, &failed_self_locks, monitor, &recv_cnt] (const std::string& /*address*/, uint64_t group_id, uint64_t node_id) {
        // New data has arrived - check buffer map
        monitor->QueueMetric("timeslice_forwarder_state",
                        {{"host", std::to_string(par.node_id) + " - " + std::to_string(par.group_id)}},
                        {{"recv_cnt", ++recv_cnt}});
        cout << "New data from Node ID: " << node_id << " - Group ID: " << group_id << endl;
        node_connector->lock_buffer_map(data_buffer_map, [data_buffer_map, node_connector, ts_sink, monitor] () {
            auto *el = data_buffer_map->get_oldest_linked_list_element(nullptr, BufferMap::ListElement::IO::RX);
            if (el == nullptr) { // not expected to happen in the current implementation
                node_connector->unlock_buffer_map(data_buffer_map);
                return;
            }
            uint64_t component_size = 0;
            auto component = data_buffer_map->get_elements_of_component(el->compontent_id, component_size);
            monitor->QueueMetric("timeslice_forwarder_state",
                {{"host", std::to_string(par.node_id) + " - " + std::to_string(par.group_id)}},
                {{"bytes_received", component_size}});
            ts_sink->write_timeslice(component);
            data_buffer_map->remove_elements(component);
            node_connector->unlock_buffer_map(data_buffer_map);
        }, [&failed_self_locks, monitor] () {
            monitor->QueueMetric("timeslice_forwarder_state",
                        {{"host", std::to_string(par.node_id) + " - " + std::to_string(par.group_id)}},
                        {{"failed_self_locks", ++failed_self_locks}});
            cout << "failed self lock: " << failed_self_locks << endl;
            return true;
        });
    });
    node->start();

    cout << "Connecting to CM at: " << cm_address << endl;
    node->connect_to_node(cm_address);
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
    } else if (FI_MAJOR_VERSION != 2 || FI_MINOR_VERSION < 2) {
        cerr << "Libfabric: invalid version." << endl;
        cerr << "Minimum version required 2.2 - found " << FI_MAJOR_VERSION << "." << FI_MINOR_VERSION << endl;
    }

    if (par.group_id == 0) {
        start_cm();
    } else if (par.group_id == 1) {
        start_sender();
    } else if (par.group_id == 2) {
        start_receiver();
    } else { // Should never happen
        cerr << "! Was unable to determine if node is sender, receiver or central manager" << endl;
    }

    return 0;
}
