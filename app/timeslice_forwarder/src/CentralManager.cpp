#include "CentralManager.hpp"
#include "log.hpp"

using namespace std::placeholders;
using namespace std;

void CentralManager::on_node_connected(std::string address, uint64_t group_id, uint64_t node_id) {
    L_(info) << "Node connected: \n" <<
            "Node ID: " << node_id  << '\n' <<
            "Group ID: " << group_id;

    auto node_uid = MAKE_UID(group_id, node_id);
    if (group_id == 1) {
        monitor_->QueueMetric("timeslice_forwarder_state",
            {{"CM", "CM"}},
            {{"input_nodes_cnt", ++input_nodes_cnt_}});
    } else {
        monitor_->QueueMetric("timeslice_forwarder_state",
            {{"CM", "CM"}},
            {{"output_nodes_cnt", ++output_nodes_cnt_}});
    }
    unique_lock<mutex> l(mtx_);
    uid_address_map_[node_uid] = address;
    connection_manager_.add_node(node_uid);
    if (uid_listen_address_map_.find(node_uid) != uid_listen_address_map_.end()) {
        connect_nodes(node_uid);
    }
}

void CentralManager::on_node_disconnected(std::string address, uint64_t group_id, uint64_t node_id) {
    unique_lock<mutex> l(mtx_);
    auto node_uid = MAKE_UID(group_id, node_id);
    auto key_pos = uid_address_map_.find(node_uid);
    uid_address_map_.erase(key_pos);
    connection_manager_.remove_node(node_uid);
    if (group_id == 1) {
        monitor_->QueueMetric("timeslice_forwarder_state",
            {{"CM", "CM"}},
            {{"input_nodes_cnt", --input_nodes_cnt_}});
    } else {
        monitor_->QueueMetric("timeslice_forwarder_state",
            {{"CM", "CM"}},
            {{"output_nodes_cnt", --output_nodes_cnt_}});
    }
}

void CentralManager::on_new_work_item(std::string /*address*/, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id) {
    auto node_uid = MAKE_UID(group_id, node_id);
    L_(debug) << "new work item: " << node_id << " - group_id " << group_id;

    if (wi_type == WorkItem::connector_config) { // The given node informed us about its connection possibilities
        L_(debug) << "connector config";
        WiConnectorConfig conn_config;
        conn_config.deserialize(wi_ptr);
        unique_lock<mutex> l(mtx_);
        uid_listen_address_map_[node_uid] = conn_config.listen_addr;
        connect_nodes(node_uid);
    } else if (wi_type == WorkItem::connection) { // The given node informed us about a new available connection
        L_(debug) << "connection";
        WiConnection wi_connection;
        wi_connection.deserialize(wi_ptr);
        auto from = MAKE_UID(wi_connection.from_group_id, wi_connection.from_node_id);
        auto to = MAKE_UID(wi_connection.to_group_id, wi_connection.to_node_id);
        unique_lock<mutex> l(mtx_);
        connection_manager_.connect_unidirectional(from, to);
    } else if (wi_type == WorkItem::buffer_status) { // The told us, that its buffer map has changed
        L_(debug) << "buffer status";
        unique_lock<mutex> l(mtx_);
        nodes_with_buffer_change_->push_back(node_uid);
        eval_worker_cv_.notify_all();
    } else {
        L_(warning) << "Received unknown WorkItem type: " << wi_type;
    }
}

void CentralManager::connect_nodes(uint64_t node_uid) {
        uint64_t group_id = GROUP_ID(node_uid);
        uint64_t node_id = GROUP_ID(node_uid);
        auto all_possible_connections = connection_manager_.get_connections(node_uid, false);
        L_(debug) << "all_possible_connections.size(): " << all_possible_connections.size();
        for (auto &remote_uid : all_possible_connections) {
            if (GROUP_ID(remote_uid) == group_id + 1 || GROUP_ID(remote_uid) == group_id - 1) {
                L_(debug) << "tell N: " << node_id << " - G: " << group_id << " ---> N: "<< NODE_ID(remote_uid) << " - G: " << GROUP_ID(remote_uid);
                auto wi_connection = make_shared<WiConnection>();
                wi_connection->type = WorkItem::connection_req;
                wi_connection->connector_uid = 0;
                wi_connection->connector_address = uid_listen_address_map_[remote_uid];
                Node::send_work_item(uid_address_map_[node_uid], wi_connection);
            }
        }
}

void CentralManager::eval_node_status(uint64_t group_id, uint64_t node_id) {
    const auto node_uid = MAKE_UID(group_id, node_id);
    unique_lock<mutex> l(mtx_);

    if (group_id == 1) { // TS sender
        auto connections = connection_manager_.get_connections(node_uid);
        if (connections.empty()) {
            L_(debug) << "no connections available";
            return;
        }
        target_idx_++;
        auto target_node_uid = connections[target_idx_ % connections.size()];
        auto target_node_id  = NODE_ID(target_node_uid);
        auto target_group_id  = GROUP_ID(target_node_uid);
        L_(info) << "Planing to send data from N:" << node_id << " - G: " << group_id  << " to N: " << target_node_id << " - G: " << target_group_id;
        auto wi_tx = make_shared<WiTransmission>();
        wi_tx->node_uid = target_node_uid;
        wi_tx->type = WorkItem::transmission;
        Node::send_work_item(uid_address_map_[node_uid], wi_tx);
    } else if (group_id == 2) { // TS receiver
        L_(debug) << "evaluation of TS receiver status" << endl;
    } else {
        L_(warning) << "! Invalid Group ID: " << group_id << '\n' <<
                "! Nodes should either have Group ID 1 or 2";
    }
}

void CentralManager::eval_thread() {
    vector<uint64_t> nodes_cpy;
    while (!stop_eval_worker) {
        {
            unique_lock<mutex> l(mtx_);
            eval_worker_cv_.wait(l, [&] () {
                return !nodes_with_buffer_change_->empty() || stop_eval_worker;
            });
            nodes_cpy = *nodes_with_buffer_change_;
            nodes_with_buffer_change_->clear();
        }
        // fetch_buffer_maps(nodes_cpy);
        for (auto const& uid : nodes_cpy) {
            eval_node_status(GROUP_ID(uid), NODE_ID(uid));
        }
    }
}

CentralManager::CentralManager(std::string listen_address, std::string monitoring_uri) : Node(0, 0), listen_address_(listen_address) {
    if (!monitoring_uri.empty()) {
        monitor_->OpenSink(monitoring_uri);
    }
    eval_worker_ = async(launch::async, &CentralManager::eval_thread, this);
    Node::on_node_connected(bind(&CentralManager::on_node_connected, this, _1, _2, _3));
    Node::on_node_disconnected(bind(&CentralManager::on_node_disconnected, this, _1, _2, _3));
    Node::on_new_work_item(bind(&CentralManager::on_new_work_item, this, _1, _2, _3, _4, _5));

    shared_ptr<ConnectorInterface> node_connector = make_shared<ConnectorInfiniband>();

    const auto wi_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    const auto wi_buffer = shared_ptr<char>(new char[WI_BUFFER_SIZE], default_delete<char[]>());

    const auto data_buffer = shared_ptr<char>(new char[DATA_BUFFER_SIZE], default_delete<char[]>());
    const auto data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, DATA_BUFFER_SIZE);

    Node::set_wi_buffer(wi_buffer, wi_buffer_map, WI_BUFFER_SIZE);
    Node::set_data_buffer(data_buffer, data_buffer_map, DATA_BUFFER_SIZE);
    Node::add_connector(node_connector, listen_address_);

    Node::start();
}
