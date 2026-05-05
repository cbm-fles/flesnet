#pragma once
#include "Monitor.hpp"
#include <cstdint>
#include <df/ConnectionManager.hpp>
#include <df/Node.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <df/WorkItems/WiTransmission.hpp>
#include <memory>

using namespace std::placeholders;
using namespace std;

class CentralManager : public Node {
private:
    std::mutex mtx;
    shared_ptr<cbm::Monitor> monitor{make_shared<cbm::Monitor>()};
    // maps used to translate node UID to IP addresses
    unordered_map<uint64_t, std::string> uid_address_map;
    unordered_map<uint64_t, std::string> uid_listen_address_map;

    // used to translate between node uid and its WI buffer map
    unordered_map<uint64_t, std::shared_ptr<BufferMap>> uid_buffer_map_map;
    shared_ptr<vector<uint64_t>> nodes_with_buffer_change{make_shared<vector<uint64_t>>()};

    // WorkerThread worker;
    ConnectionManager connection_manager; // used to take track of connections between nodes
    condition_variable eval_worker_cv;
    future<void> eval_worker;
    atomic_bool stop_eval_worker = false;

    atomic_uint16_t target_idx = 0;
    atomic_uint64_t input_nodes_cnt = 0;
    atomic_uint64_t output_nodes_cnt = 0;

    void on_node_connected(std::string address, uint64_t group_id, uint64_t node_id) {
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
        if (uid_listen_address_map.find(node_uid) != uid_listen_address_map.end()) {
            connect_nodes(node_uid);
        }
    }

    void on_node_disconnected(std::string address, uint64_t group_id, uint64_t node_id) {
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
    }

    void connect_nodes(uint64_t node_uid) {
            // auto node_uid = MAKE_UID(group_id, node_id);
            uint64_t group_id = GROUP_ID(node_uid);
            uint64_t node_id = GROUP_ID(node_uid);
            auto all_possible_connections = connection_manager.get_connections(node_uid, false);
            // vector<uint64_t> relevant_connections;
            // cout << "relevant connections: " << relevant_connections.size() << endl;
            cout << "all_possible_connections.size(): " << all_possible_connections.size() << endl;
            for (auto &remote_uid : all_possible_connections) {
                if (GROUP_ID(remote_uid) == group_id + 1 || GROUP_ID(remote_uid) == group_id - 1) {
                    std::cout << "tell N: " << node_id << " - G: " << group_id << " ---> N: "<< NODE_ID(remote_uid) << " - G: " << GROUP_ID(remote_uid) << std::endl;
                    auto wi_connection = make_shared<WiConnection>();
                    wi_connection->type = WorkItem::connection_req;
                    wi_connection->connector_uid = 0;
                    wi_connection->connector_address = uid_listen_address_map[remote_uid];
                    std::cout << "send work item" << std::endl;
                    Node::send_work_item(uid_address_map[node_uid], wi_connection);
                }
            }
    }

    void on_new_work_item(std::string /*address*/, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id) {
        auto node_uid = MAKE_UID(group_id, node_id);
        cout << "new work item: " << node_id << " - group_id " << group_id << endl;

        if (wi_type == WorkItem::connector_config) { // The given node informed us about its connection possibilities
            cout << "-- connector config" << endl;
            WiConnectorConfig conn_config;
            conn_config.deserialize(wi_ptr);
            unique_lock<mutex> l(mtx);
            uid_listen_address_map[node_uid] = conn_config.listen_addr;
            connect_nodes(node_uid);
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
    }

    void eval_node_status(uint64_t group_id, uint64_t node_id) {
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
            Node::send_work_item(uid_address_map[node_uid], wi_tx);
        } else if (group_id == 2) { // TS receiver
            cout << "evaluation of TS receiver status" << endl;
        } else {
            cerr << "! Invalid Group ID: " << group_id << '\n' <<
                    "! Nodes should either have Group ID 1 or 2" << endl;
        }
    }

    void eval_thread() {
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
    }

public:
    CentralManager(std::string monitoring_uri = "") : Node(0, 0) {
        if (!monitoring_uri.empty()) {
            monitor->OpenSink(monitoring_uri);
        }
        eval_worker = std::async(launch::async, &CentralManager::eval_thread, this);
        Node::on_node_connected(std::bind(&CentralManager::on_node_connected, this, _1, _2, _3));
        Node::on_node_disconnected(std::bind(&CentralManager::on_node_disconnected, this, _1, _2, _3));
        Node::on_new_work_item(std::bind(&CentralManager::on_new_work_item, this, _1, _2, _3, _4, _5));
    }
};