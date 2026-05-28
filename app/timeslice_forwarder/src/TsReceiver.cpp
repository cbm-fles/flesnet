#include "WorkItems.hpp"
#include <TsReceiver.hpp>
#include <TsclientWriter.hpp>
#include <chrono>
#include <cstdint>
#include <thread>

using namespace std;
using namespace std::placeholders;


void TsReceiver::on_new_work_item(std::string /*address*/, std::shared_ptr<char> /*wi_ptr*/, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id) {
    if (group_id == 0 && node_id == 0) { // received new work item from central manager

    } else {
        L_(info) << "Received work item: " << wi_type << endl;
    }
}

void TsReceiver::on_node_connected(string address, uint64_t rem_group_id, uint64_t rem_node_id) {
    L_(info) << "Node connected: \n" <<
            "Group ID: " << rem_group_id << '\n' <<
            "Node ID: " << rem_node_id;

    if (rem_group_id == 0 && rem_node_id == 0) { // connected to central manager - tell it about our connection possibilities
        auto conn_config = make_shared<WiConnectorConfig>();
        conn_config->type = WorkItem::connector_config;
        conn_config->connector_uid = 0;
        conn_config->listen_addr = node_listen_addr_;
        conn_config->name = "ConnectorInfiniband";
        Node::send_work_item(address, conn_config);
        // auto wi_buffer_request = make_shared<WiBufferRequest>();
        // Node::send_work_item(address, wi_buffer_request);
    } else { // Connected to some other node - tell the central manager about it
        auto wi_connection = make_shared<WiConnection>();
        wi_connection->type = WorkItem::connection;
        wi_connection->from_group_id = group_id_;
        wi_connection->from_node_id = node_id_;
        wi_connection->to_group_id = rem_group_id;
        wi_connection->to_node_id = rem_node_id;
        Node::send_work_item(cm_address_, wi_connection, [] () {
            L_(debug) << "send work item done (WorkItem::connection)";
        });
    }
}

void TsReceiver::on_connection_refused(std::string address) {
    if (address == cm_address_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        Node::connect_to_node(cm_address_);
    }
};

void TsReceiver::on_new_data (const std::string& /*address*/, uint64_t group_id, uint64_t node_id) {
    // New data has arrived - check buffer map
    monitor_->QueueMetric("timeslice_forwarder_state",
                    {{"host", std::to_string(node_id_) + " - " + std::to_string(group_id_)}},
                    {{"recv_cnt", ++recv_cnt_}});
    // L_(info)  << "New data from Node ID: " << node_id << " - Group ID: " << group_id;
    // this_thread::sleep_for(chrono::milliseconds(500));
    node_connector_->lock_buffer_map(data_buffer_map_, [this] () {
        L_(debug) << "BUFFER MAP LOCKED - on_new_data";
        // auto *el = data_buffer_map_->get_latest_linked_list_element(nullptr, BufferMap::ListElement::IO::RX);
        auto *el = data_buffer_map_->get_oldest_linked_list_element(nullptr, BufferMap::ListElement::IO::RX);
        if (el == nullptr) { // not expected to happen in the current implementation
            node_connector_->unlock_buffer_map(data_buffer_map_);
            return;
        }
        uint64_t component_size = 0;
        auto component = data_buffer_map_->get_elements_of_component(el->compontent_id, component_size);
        for (auto &component : component) {
            component->rx_tx = BufferMap::ListElement::IO::UNSPEC; // asynchronousity makes it possible to read it twice, therefore we remove the RX mark to prevent this from happening
        }
        L_(info)  << "New data from Node ID: " << component[0]->node_id << " - Group ID: " << component[0]->group_id;

        monitor_->QueueMetric("timeslice_forwarder_state",
            {{"host", std::to_string(node_id_) + " - " + std::to_string(group_id_)}},
            {{"bytes_received", component_size}});
        L_(debug) << "write_timeslice start";
        ts_sink_->write_timeslice(component);
        L_(debug) << "write_timeslice done";
        node_connector_->unlock_buffer_map(data_buffer_map_);
        L_(debug) << "BUFFER MAP UNlocked  - on_new_data";

    }, [this] () {
        monitor_->QueueMetric("timeslice_forwarder_state",
                    {{"host", std::to_string(node_id_) + " - " + std::to_string(group_id_)}},
                    {{"failed_self_locks", ++failed_self_locks_}});
        return true;
    });
}

TsReceiver::TsReceiver(uint64_t node_id,
    std::string listen_address,
    std::string output_uri,
    uint32_t timeslice_size,
    std::string central_manager_address,
    std::string monitoring_uri) :
Node(node_id, 2), cm_address_(central_manager_address), node_listen_addr_(listen_address) {
    if (!monitoring_uri.empty()) {
        monitor_->OpenSink(monitoring_uri);
    }
    wi_work_done_ = make_shared<WiWorkDone>();
    ts_sink_ = make_shared<TsclientWriter>(output_uri, timeslice_size);
    data_buffer_ = ts_sink_->get_buffer();
    data_buffer_size_ = ts_sink_->get_buffer_size();
    data_buffer_map_ = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, data_buffer_size_);
    ts_sink_->on_timeslices_handled([this] (uint64_t /*finished_ts_cnt*/) {
        L_(trace) << "on_timeslices_handled";
        if (ts_sink_->get_finished_component_id_cnt() == 0) {
            return;
        }
        node_connector_->lock_buffer_map(data_buffer_map_, [this] () {
            L_(debug) << "BUFFER MAP LOCKED - on_timeslices_handled";

            uint64_t component_id;
            L_(trace) << "on_timeslices_handled - lock_buffer_map:";
            while (ts_sink_->pop_finished_component_id(component_id)) {
                L_(trace) << "on_timeslices_handled - component_id: " << component_id;

                uint64_t combined_size;
                auto a = data_buffer_map_->get_elements_of_component(component_id, combined_size);
                L_(debug) << "a.size(): " << a.size();
                if (a.size() != 52) {
                    L_(fatal) << "component size not 52: " << a.size();
                }
                // data_buffer_map_->print_all();
                data_buffer_map_->remove_elements(a);
                Node::send_work_item(cm_address_, wi_work_done_);
            }
            node_connector_->unlock_buffer_map(data_buffer_map_);
            L_(debug) << "BUFFER MAP UNlocked  - on_timeslices_handled";
        }, [] () {
            return true;
        });
    });
    /**
    * The shared memory is represented by boost::managed_shared_memory.
    * Boost seems to store some metadata for its management in the SHM too.
    * It seems to be constant 336 byte. Therefore this needs to be represented in the buffer map too.
    */
    data_buffer_map_->insert(0, 336, node_id_, group_id_, BufferMap::TAG_UNSET);

    wi_buffer_map_ = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    wi_buffer_ = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());

    node_connector_ = make_shared<ConnectorInfiniband>();

    Node::set_wi_buffer(wi_buffer_, wi_buffer_map_, WI_BUFFER_SIZE);
    Node::set_data_buffer(data_buffer_, data_buffer_map_, data_buffer_size_);
    Node::add_connector(node_connector_, node_listen_addr_);
    Node::on_new_data(std::bind(&TsReceiver::on_new_data, this, _1, _2, _3));
    Node::on_new_work_item(std::bind(&TsReceiver::on_new_work_item, this, _1, _2, _3, _4, _5));
    Node::on_node_connected(std::bind(&TsReceiver::on_node_connected, this, _1, _2, _3));
    Node::on_connection_refused(std::bind(&TsReceiver::on_connection_refused, this, _1));
    Node::start();
    Node::connect_to_node(cm_address_);
}