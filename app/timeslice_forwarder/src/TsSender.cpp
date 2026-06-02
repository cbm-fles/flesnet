#include <atomic>
#include <cstdint>
#include <df/ConnectionManager.hpp>
#include <df/Node.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <df/WorkItems/WiTransmission.hpp>
#include <memory>
#include "TsclientReader.hpp"
#include "WorkItems.hpp"
#include "df/WorkItems/WorkItem.hpp"
#include <df/Connectors/ConnectorInfiniband.hpp>
#include <df/Connectors/ConnectorInfiniband.hpp>
#include <TsSender.hpp>

using namespace std::placeholders;
using namespace std;

void TsSender::send_latest_data(uint64_t group_id, uint64_t node_id) {
    string rem_address;
    auto node_uid = MAKE_UID(group_id, node_id);
    {
        shared_lock<shared_mutex> l(mtx_);
        rem_address = uid_address_map_[node_uid];
    }

    L_(info) << "Commanded to send data to Node ID: " << node_id << " - Group ID: " << group_id << " - address: " << rem_address;
    node_connector_->lock_buffer_map(data_buffer_map_, [this, rem_address, node_id, group_id] () {
        auto *el = data_buffer_map_->get_oldest_linked_list_element(nullptr, BufferMap::ListElement::IO::RX);
        auto buffer_fill_state =(static_cast<double>(data_buffer_map_->get_list_metadata()->used_mem) / static_cast<double>(data_buffer_map_->get_list_metadata()->buffer_size)) * 100.0;
        auto buffer_map_fill_state = (static_cast<double>(data_buffer_map_->get_list_metadata()->element_cnt - data_buffer_map_->get_list_metadata()->available_element_cnt) / static_cast<double>(data_buffer_map_->get_list_metadata()->element_cnt)) * 100.0;
        monitor_->QueueMetric("timeslice_forwarder_state",
        {
                {"host", hostname_},
            {"sender", to_string(node_id_)}
        },
            {
                {"buffer_fill", buffer_fill_state},
                {"buffer_map_fill", buffer_map_fill_state},
            }
        );
        if (el == nullptr) { // no oldest element available
            L_(debug) << "No data available to send";
            node_connector_->unlock_buffer_map(data_buffer_map_);
            return;
        }
        uint64_t combined_size = 0;
        auto component_elements = data_buffer_map_->get_elements_of_component(el->compontent_id, combined_size);
        auto *data_write_chain = new std::function<void()>;
        (*data_write_chain) = [this, data_write_chain, rem_address, component_elements, combined_size, node_id, group_id] () {
            atomic_uint64_t fail_cnt = 0;
            node_connector_->lock_and_get_buffer_map(
                rem_address,
                Node::DATA_BUFFER_IDX,
                [this, data_write_chain, component_elements, rem_address, combined_size, node_id, group_id] (shared_ptr<BufferMap> rem_buffer_map_copy) {
                    auto rem_offsets_and_spaces = rem_buffer_map_copy->get_offsets_and_spaces();
                    auto dest_addresses = eval_logic_.evaluate(component_elements, rem_offsets_and_spaces);
                    if (dest_addresses.empty()) {
                        L_(warning) << "Remote buffer full - Node ID: " << node_id << " - Group ID: " << group_id;
                        auto wi_buffer_full_report = make_shared<WiBufferFullReport>();
                        wi_buffer_full_report->node_id = node_id;
                        wi_buffer_full_report->group_id = group_id;
                        send_work_item(cm_address_, wi_buffer_full_report);
                        node_connector_->unlock_remote_buffer_map(
                            rem_address,
                            rem_buffer_map_copy,
                            Node::DATA_BUFFER_IDX
                        );
                        node_connector_->unlock_buffer_map(data_buffer_map_);
                        monitor_->QueueMetric("timeslice_forwarder_state",
                                    {{"host", hostname_},
                                        {"sender", to_string(node_id_)}},
                                    {{"rem_buffer_full", ++rem_buffer_full_cnt_}});
                        delete data_write_chain;
                        return;
                    }

                    vector<uint64_t> src_mem_addresses;
                    vector<uint64_t> sizes;
                    src_mem_addresses.resize(component_elements.size());
                    sizes.resize(component_elements.size());
                    for (uint64_t i = 0; i < component_elements.size(); i++) {
                        src_mem_addresses[i] = component_elements[i]->address;
                        sizes[i] = component_elements[i]->len;
                    }

                    bool insert_successfull = rem_buffer_map_copy->insert(component_elements, dest_addresses, node_id_, group_id_, BufferMap::ListElement::RX);
                    if (!insert_successfull) {
                        L_(warning) << "Remote buffer map has no elements available - Node ID: " << node_id << " - Group ID: " << group_id;
                        auto wi_buffer_full_report = make_shared<WiBufferFullReport>();
                        wi_buffer_full_report->node_id = node_id;
                        wi_buffer_full_report->group_id = group_id;
                        send_work_item(cm_address_, wi_buffer_full_report);
                        node_connector_->unlock_remote_buffer_map(
                            rem_address,
                            rem_buffer_map_copy,
                            Node::DATA_BUFFER_IDX
                        );
                        node_connector_->unlock_buffer_map(data_buffer_map_);
                        monitor_->QueueMetric("timeslice_forwarder_state",
                                    {{"host", hostname_},
                                        {"sender", to_string(node_id_)}},
                                    {{"rem_buffer_map_full", ++rem_buffer_map_full_cnt_}});

                        delete data_write_chain;
                        return;
                    }
                    delete data_write_chain;

                    node_connector_->sendv(
                        rem_address,
                        data_buffer_,
                        Node::DATA_BUFFER_IDX,
                        src_mem_addresses,
                        dest_addresses,
                        sizes,
                        [this, rem_address, rem_buffer_map_copy, component_elements, combined_size] () {
                            // send the new buffer map to remote node and unlock
                            ts_reader->clear_last_timeslice();
                            node_connector_->write_remote_buffer_map_and_unlock(rem_address, rem_buffer_map_copy,
                                Node::DATA_BUFFER_IDX,
                                [this, component_elements, combined_size, rem_address] () {
                                    monitor_->QueueMetric("timeslice_forwarder_state",
                                        {{"host", hostname_},
                                            {"sender", to_string(node_id_)}},
                                        {{"send_cnt", ++send_cnt_}});
                                    monitor_->QueueMetric("timeslice_forwarder_state",
                                        {{"host", hostname_},
                                            {"sender", to_string(node_id_)}},
                                        {{"bytes_sent", combined_size}});

                                    // remove the sent TS from own buffermap
                                    data_buffer_map_->remove_elements(component_elements);
                                    node_connector_->unlock_buffer_map(data_buffer_map_);
                                }
                            );
                        }
                    );
                },
                [this, &fail_cnt] () {
                    if ((++fail_cnt % 200) == 0) {
                        L_(debug) << "failed to get remote lock";
                    };
                    monitor_->QueueMetric("timeslice_forwarder_state",
                            {{"host", hostname_},
                                {"sender", to_string(node_id_)}},
                        {{"failed_remote_lock", ++failed_remote_lock_cnt_}});
                    return true;
                }
            );
        };
        (*data_write_chain)();
    }, [] () {
        return true;
    });
}

void TsSender::on_new_work_item(std::string /*address*/, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id) {
    if (group_id == 0 && node_id == 0) { // received new work item from central manager
        if (wi_type == WorkItem::transmission) { // CM told us to send data to a specific node
            WiTransmission wi_transmission;
            wi_transmission.deserialize(wi_ptr);
            auto remote_node_id = NODE_ID(wi_transmission.node_uid);
            auto remote_group_id = GROUP_ID(wi_transmission.node_uid);
            send_latest_data(remote_group_id, remote_node_id);
        }
    }
}

void TsSender::on_node_connected(string address, uint64_t rem_group_id, uint64_t rem_node_id) {
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
        //! @todo figure out the race condition that makes this timeout necessary
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        ts_reader->start_timeslice_reading();
    } else { // Connected to some other node - tell the central manager about it
        auto const node_uid = MAKE_UID(rem_group_id, rem_node_id);
        auto wi_connection = make_shared<WiConnection>();
        wi_connection->type = WorkItem::connection;
        wi_connection->from_group_id = group_id_;
        wi_connection->from_node_id = node_id_;
        wi_connection->to_group_id = rem_group_id;
        wi_connection->to_node_id = rem_node_id;
        {
            unique_lock<shared_mutex> l(mtx_);
            uid_address_map_[node_uid] = address;
        }
        Node::send_work_item(cm_address_, wi_connection);
    }
}

void TsSender::on_connection_refused(std::string address) {
    if (address == cm_address_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Node::connect_to_node(cm_address_);
    }
};

TsSender::TsSender(uint64_t node_id,
    std::string listen_address,
    std::string input_uri,
    std::string central_manager_address,
    std::string monitoring_uri,
    std::string hostname) :
Node(node_id, 1), cm_address_(central_manager_address), node_listen_addr_(listen_address), hostname_(hostname) {
    if (!monitoring_uri.empty()) {
        monitor_->OpenSink(monitoring_uri);
    }
    ts_reader = make_shared<TsclientReader>(input_uri);

    data_buffer_size_  = ts_reader->get_buffer_size();
    data_buffer_ = shared_ptr<char>(ts_reader->get_buffer());
    data_buffer_map_ = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, data_buffer_size_);
    ts_reader->set_buffer_map(data_buffer_map_);

    wi_buffer_map_ = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    wi_buffer_ = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());
    node_connector_ = make_shared<ConnectorInfiniband>();

    Node::add_connector(node_connector_, node_listen_addr_);
    Node::set_wi_buffer(wi_buffer_, wi_buffer_map_, WI_BUFFER_SIZE);
    Node::set_data_buffer(data_buffer_, data_buffer_map_, data_buffer_size_);

    Node::on_new_work_item(std::bind(&TsSender::on_new_work_item, this, _1, _2, _3, _4, _5));
    Node::on_node_connected(std::bind(&TsSender::on_node_connected, this, _1, _2, _3));
    Node::on_connection_refused(std::bind(&TsSender::on_connection_refused, this, _1));

    wi_buffer_status_ = make_shared<WorkItem>();
    wi_buffer_status_->type = WorkItem::buffer_status;
    ts_reader->on_new_timeslice([this] () {
        Node::send_work_item(cm_address_, wi_buffer_status_);
    });

    ts_reader->set_node_connector(node_connector_);
    Node::start();
    Node::connect_to_node(cm_address_);
}
