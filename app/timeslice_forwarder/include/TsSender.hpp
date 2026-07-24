#pragma once

#include <df/ConnectionManager.hpp>
#include <df/Node.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <df/WorkItems/WiTransmission.hpp>
#include <df/WorkItems/WorkItem.hpp>

#include "LogVar.hpp"
#include "Monitor.hpp"
#include "TsclientReader.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

/**
 * @brief Timeslice Sender Node implementation
 */
class TsSender : public Node {

private:
    const uint64_t BUFFER_MAP_ELEMENTS = 512;
    const uint64_t WI_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 5;

    std::shared_mutex mtx_;
    std::shared_ptr<cbm::Monitor> monitor_{std::make_shared<cbm::Monitor>()};
    std::unordered_map<uint64_t, std::string> uid_address_map_;
    EvaluationLogic eval_logic_;
    std::string cm_address_;
    std::shared_ptr<TsclientReader> ts_reader = nullptr;
    uint64_t data_buffer_size_ = 0;
    std::shared_ptr<char> data_buffer_ = nullptr;
    std::shared_ptr<BufferMap> data_buffer_map_ = nullptr;

    std::shared_ptr<char> wi_buffer_ = nullptr;
    std::shared_ptr<BufferMap> wi_buffer_map_ = nullptr;
    std::shared_ptr<ConnectorInterface> node_connector_ = nullptr;
    std::string node_listen_addr_;
    std::shared_ptr<WorkItem> wi_buffer_status_ = nullptr;

    std::future<void> log_thread_;
    Avg<std::atomic<double>> bytes_sent_;

    /**
    * @brief values used for influxDB/grafana monitoring
    */
    std::string hostname_; // own hostname
    std::atomic_uint64_t failed_remote_lock_cnt_ = 0; // couts how many time remote buffer map lock failed
    std::atomic_uint64_t send_cnt_ = 0; // counts how many tranmissions were performed
    std::atomic_uint64_t rem_buffer_full_cnt_ = 0; // counts how many times the remote buffer was full when trying to transmit data
    std::atomic_uint64_t rem_buffer_map_full_cnt_ = 0; // counts how many times the remote buffer map was out of elements when trying to transmit data

    /**
     * @brief Callback for newly received work items
     * @param address - Address of the sender node
     * @param wi_ptr - Pointer of the serialized workitem
     * @param wi_tyoe - Type of the work item (see field `type` of WorkItem class)
     * @param group_id - Group ID of the sender node
     * @param node_id - Node ID of the sender node
     */
    void on_new_work_item(std::string address, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id);

    /**
     * @brief Used as callback for successfull connections
     * @param address - Address of the new connection
     * @param rem_group_id - Group ID of the connected node
     * @param rem_node_id - Node ID of the connected node
     */
    void on_node_connected(std::string address, uint64_t rem_group_id, uint64_t rem_node_id);

    /**
     * @brief Used as callback for refused connections
     * @param address - address of which the connection got refused 
     */
    void on_connection_refused(std::string address);

    /**
     * @brief Sends the latest inserted data from the buffer map to the given node
     * @param group_id - Group ID of the target node
     * @param node_id - Node ID of the target node
     */
    void send_latest_data(uint64_t group_id, uint64_t node_id);

    /**
     * @brief returns if the connection to the Central Manager is available
     * @todo implement - Currenty returns hardcoded value
     */
    bool is_cm_available();

    // debugging variables to internall meassure performance
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
    std::chrono::time_point<std::chrono::high_resolution_clock> stop_;

public:
    TsSender(
        uint64_t node_id,
        std::string listen_address,
        std::string input_uri,
        std::string central_manager_address,
        std::string monitoring_uri = "",
        std::string hostname = ""
    );
 };
