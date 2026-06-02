#include "WorkItems.hpp"
#include <TsReceiver.hpp>
#include <TsclientWriter.hpp>
#include <chrono>
#include <cstdint>
#include <thread>

using namespace std;
using namespace std::placeholders;
using namespace std::chrono;


void TsReceiver::on_new_work_item(std::string /*address*/, std::shared_ptr<char> /*wi_ptr*/, WorkItem::Type /*wi_type*/, uint64_t group_id, uint64_t node_id) {
    L_(warning) << "Received work item from Node ID: " << node_id << " - " << " - Group ID: " << group_id << " - no handler implemented on receiver side";
}

void TsReceiver::on_node_connected(string address, uint64_t rem_group_id, uint64_t rem_node_id) {
    L_(info) << "Node connected: " << endl <<
            "Group ID: " << rem_group_id << endl <<
            "Node ID: " << rem_node_id;
    if (rem_group_id == 0 && rem_node_id == 0) { // connected to central manager - tell it about our connection possibilities
        auto conn_config = make_shared<WiConnectorConfig>();
        conn_config->type = WorkItem::connector_config;
        conn_config->connector_uid = 0;
        conn_config->listen_addr = node_listen_addr_;
        conn_config->name = "ConnectorInfiniband";
        Node::send_work_item(address, conn_config);
    } else { // Connected to some other node - tell the central manager about it
        auto wi_connection = make_shared<WiConnection>();
        wi_connection->type = WorkItem::connection;
        wi_connection->from_group_id = group_id_;
        wi_connection->from_node_id = node_id_;
        wi_connection->to_group_id = rem_group_id;
        wi_connection->to_node_id = rem_node_id;
        Node::send_work_item(cm_address_, wi_connection);
    }
}

void TsReceiver::on_connection_refused(std::string address) {
    if (address == cm_address_) { // connection refused
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        Node::connect_to_node(cm_address_);
    } else {
        L_(fatal) << "Connection refused: Node with address '" << address << "' not available";
    }
};

void TsReceiver::on_new_data (const std::string& /*address*/, uint64_t group_id, uint64_t node_id) {
    // New data has arrived - check buffer map
    node_connector_->lock_buffer_map(data_buffer_map_, [this] () {
        time_point<high_resolution_clock> start;
        time_point<high_resolution_clock> stop;
        start = high_resolution_clock::now();
        L_(info) << "TS writer - ts written after: " <<  duration_cast<milliseconds>(stop-start).count();

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
        ts_sink_->write_timeslice(component);
        auto buffer_fill_state =(static_cast<double>(data_buffer_map_->get_list_metadata()->used_mem) / static_cast<double>(data_buffer_map_->get_list_metadata()->buffer_size)) * 100.0;
        auto buffer_map_fill_state = (static_cast<double>(data_buffer_map_->get_list_metadata()->element_cnt - data_buffer_map_->get_list_metadata()->available_element_cnt) / static_cast<double>(data_buffer_map_->get_list_metadata()->element_cnt)) * 100.0;
        node_connector_->unlock_buffer_map(data_buffer_map_);
        stop = high_resolution_clock::now();
        L_(info) << "TS on_new_data - done after: " <<  duration_cast<milliseconds>(stop-start).count();

        monitor_->QueueMetric("timeslice_forwarder_state",
            {
                {"host", hostname_},
            {"receiver", to_string(node_id_)}
            },
            {
                {"bytes_received", component_size},
                {"buffer_fill", buffer_fill_state},
                {"buffer_map_fill", buffer_map_fill_state},
                {"recv_cnt", ++recv_cnt_}
            }
        );
    }, [this] () {
        monitor_->QueueMetric("timeslice_forwarder_state",
            {{"host", hostname_},
            {"receiver", to_string(node_id_)}},
                    {{"failed_self_locks", ++failed_self_locks_}});
        return true;
    });
}

TsReceiver::TsReceiver(uint64_t node_id,
    std::string listen_address,
    std::string output_uri,
    uint32_t timeslice_size,
    std::string central_manager_address,
    std::string monitoring_uri,
    std::string hostname) :
Node(node_id, 2), cm_address_(central_manager_address), node_listen_addr_(listen_address), hostname_(hostname) {
    if (!monitoring_uri.empty()) {
        monitor_->OpenSink(monitoring_uri);
    }
    wi_work_done_ = make_shared<WiWorkDone>();
    ts_sink_ = make_shared<TsclientWriter>(output_uri, timeslice_size);
    data_buffer_ = ts_sink_->get_buffer();
    data_buffer_size_ = ts_sink_->get_buffer_size();
    data_buffer_map_ = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, data_buffer_size_);
    ts_sink_->on_timeslices_handled([this] (uint64_t /*finished_ts_cnt*/) {
        if (ts_sink_->get_finished_component_id_cnt() == 0) {
            return;
        }
        node_connector_->lock_buffer_map(data_buffer_map_, [this] () {
            time_point<high_resolution_clock> start;
            time_point<high_resolution_clock> stop;
            start = high_resolution_clock::now();

            uint64_t component_id;
            while (ts_sink_->pop_finished_component_id(component_id)) {
                uint64_t combined_size;
                auto component = data_buffer_map_->get_elements_of_component(component_id, combined_size);
                data_buffer_map_->remove_elements(component);
                Node::send_work_item(cm_address_, wi_work_done_);
            }
            node_connector_->unlock_buffer_map(data_buffer_map_);
            stop = high_resolution_clock::now();
            L_(info) << "TS on_timeslices_handled - done after: " <<  duration_cast<milliseconds>(stop-start).count();

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