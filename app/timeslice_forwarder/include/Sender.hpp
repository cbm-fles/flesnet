#pragma once
#include "Monitor.hpp"
#include <atomic>
#include <cstdint>
#include <df/ConnectionManager.hpp>
#include <df/Node.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <df/WorkItems/WiTransmission.hpp>
#include <memory>
#include "TsclientReader.hpp"
#include "df/WorkItems/WorkItem.hpp"
#include <df/Connectors/ConnectorInfiniband.hpp>

using namespace std::placeholders;
using namespace std;
constexpr uint64_t BUFFER_MAP_ELEMENTS = 512;
constexpr uint64_t DATA_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 450;
constexpr uint64_t WI_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 5;

class Sender : public Node {
private:
    std::shared_mutex mtx_;
    shared_ptr<cbm::Monitor> monitor_{make_shared<cbm::Monitor>()};
    unordered_map<uint64_t, std::string> uid_address_map_;
    EvaluationLogic eval_logic_;
    std::string cm_address_;
    std::shared_ptr<TsclientReader> ts_reader = nullptr;
    uint64_t data_buffer_size_ = 0;
    shared_ptr<char> data_buffer_ = nullptr;
    shared_ptr<BufferMap> data_buffer_map_ = nullptr;

    shared_ptr<char> wi_buffer_ = nullptr;
    shared_ptr<BufferMap> wi_buffer_map_ = nullptr;
    shared_ptr<ConnectorInterface> node_connector_ = nullptr;
    atomic_uint64_t send_cnt_ = 0;
    atomic_uint64_t failed_remote_lock_cnt_ = 0;
    std::string node_listen_addr_;
    shared_ptr<WorkItem> wi_buffer_status_ = nullptr;

    void on_new_work_item(std::string /*address*/, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id) {
        if (group_id == 0 && node_id == 0) { // received new work item from central manager
            if (wi_type == WorkItem::transmission) { // CM told us to send data to a specific node
                WiTransmission wi_transmission;
                wi_transmission.deserialize(wi_ptr);
                auto remote_node_id = NODE_ID(wi_transmission.node_uid);
                auto remote_group_id = GROUP_ID(wi_transmission.node_uid);
                string rem_address;
                {
                    shared_lock<shared_mutex> l(mtx_);
                    rem_address = uid_address_map_[wi_transmission.node_uid];
                }

                cout << "Commanded to send data to Node ID: " << remote_node_id << " - Group ID: " << remote_group_id << " - address: " << rem_address << endl;
                auto *el = data_buffer_map_->get_oldest_linked_list_element(nullptr, BufferMap::ListElement::IO::RX);
                if (el == nullptr) { // no oldest element available
                    cout << "no oldest element available" << endl;
                    return;
                }

                uint64_t combined_size = 0;
                auto component_elements = data_buffer_map_->get_elements_of_component(el->compontent_id, combined_size);
                auto *data_write_chain = new std::function<void()>;
                (*data_write_chain) = [this, data_write_chain, rem_address, component_elements, combined_size] () {
                    node_connector_->lock_and_get_buffer_map(
                        rem_address,
                        Node::DATA_BUFFER_IDX,
                        [this, data_write_chain, component_elements, rem_address, combined_size] (shared_ptr<BufferMap> rem_buffer_map_copy) {
                            auto rem_offsets_and_spaces = rem_buffer_map_copy->get_offsets_and_spaces();
                            auto dest_addresses = eval_logic_.evaluate(component_elements, rem_offsets_and_spaces);
                            if (dest_addresses.empty()) {
                                cout << "no dest addresses caluclated" << endl;
                                node_connector_->unlock_remote_buffer_map(
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
                                cout << "!insert_successfull" << endl;
                                node_connector_->unlock_remote_buffer_map(
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
                            cout << "sendv done" << endl;

                            node_connector_->sendv(
                                rem_address,
                                data_buffer_,
                                Node::DATA_BUFFER_IDX,
                                src_mem_addresses,
                                dest_addresses,
                                sizes,
                                [this, rem_address, rem_buffer_map_copy, component_elements, combined_size] () {
                                    cout << "sendv done" << endl;
                                    // send the new buffer map to remote node and unlock
                                    node_connector_->write_remote_buffer_map_and_unlock(rem_address, rem_buffer_map_copy,
                                        Node::DATA_BUFFER_IDX,
                                        [this, component_elements, combined_size] () {
                                            monitor_->QueueMetric("timeslice_forwarder_state",
                                                {{"host", std::to_string(node_id_) + " - " + std::to_string(1)}},
                                                {{"send_cnt", ++send_cnt_}});
                                            monitor_->QueueMetric("timeslice_forwarder_state",
                                                {{"host", std::to_string(node_id_) + " - " + std::to_string(1)}},
                                                {{"bytes_sent", combined_size}});
                                            // remove the sent TS from own buffermap
                                            data_buffer_map_->remove_elements(component_elements);
                                            // call clear_timeslice on ts_reader
                                            ts_reader->clear_last_timeslice();
                                        }
                                    );
                                }
                            );
                        },
                        [this] () {
                            monitor_->QueueMetric("timeslice_forwarder_state",
                                {{"host", std::to_string(node_id_) + " - " + std::to_string(1)}},
                                {{"failed_remote_lock", ++failed_remote_lock_cnt_}});
                            return true;
                        }
                    );
                };
                (*data_write_chain)();
            }
        }
    }

    void on_node_connected(string address, uint64_t rem_group_id, uint64_t rem_node_id) {
        cout << "Node connected: \n" <<
                "Group ID: " << rem_group_id << '\n' <<
                "Node ID: " << rem_node_id  << endl;

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

            // is_cm_connected = true;
        } else { // Connected to some other node - tell the central manager about it
            auto const node_uid = MAKE_UID(rem_group_id, rem_node_id);
            auto wi_connection = make_shared<WiConnection>();
            wi_connection->type = WorkItem::connection;
            wi_connection->from_group_id = group_id_;
            wi_connection->from_node_id = node_id_;
            wi_connection->to_group_id = rem_group_id;
            wi_connection->to_node_id = rem_node_id;
            unique_lock<shared_mutex> l(mtx_);
            uid_address_map_[node_uid] = address;
            Node::send_work_item(cm_address_, wi_connection, [this] () {
                Node::send_work_item(cm_address_, wi_buffer_status_);
            });
        }
    }

    void on_connection_refused(std::string address) {
        if (address == cm_address_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            Node::connect_to_node(cm_address_);
        }
    };
public:
    Sender(uint64_t node_id,
        std::string listen_address,
        std::string shm_uri,
        std::string central_manager_address,
        std::string monitoring_uri = "") :
    Node(node_id, 1), cm_address_(central_manager_address), node_listen_addr_(listen_address) {
        if (!monitoring_uri.empty()) {
            monitor_->OpenSink(monitoring_uri);
        }
        ts_reader = make_shared<TsclientReader>(shm_uri);

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

        Node::on_new_work_item(std::bind(&Sender::on_new_work_item, this, _1, _2, _3, _4, _5));
        Node::on_node_connected(std::bind(&Sender::on_node_connected, this, _1, _2, _3));
        Node::on_connection_refused(std::bind(&Sender::on_connection_refused, this, _1));

        wi_buffer_status_ = make_shared<WorkItem>();
        wi_buffer_status_->type = WorkItem::buffer_status;
        ts_reader->on_new_timeslice([this] () {
            Node::send_work_item(cm_address_, wi_buffer_status_);
        });

        ts_reader->set_node_connector(node_connector_);
        Node::start();
        Node::connect_to_node(cm_address_);
    }


 };