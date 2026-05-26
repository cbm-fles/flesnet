#pragma once

#include "Monitor.hpp"
#include <atomic>
#include <cstdint>
#include <df/ConnectionManager.hpp>
#include <df/Node.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <df/WorkItems/WiTransmission.hpp>
#include <memory>
#include "Tssink.hpp"
#include <df/WorkItems/WorkItem.hpp>
#include <df/Connectors/ConnectorInfiniband.hpp>



class TsReceiver : public Node {
private:
    const uint64_t BUFFER_MAP_ELEMENTS = 512;
    const uint64_t WI_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 5;

    std::shared_mutex mtx_;
    std::shared_ptr<cbm::Monitor> monitor_{std::make_shared<cbm::Monitor>()};
    std::unordered_map<uint64_t, std::string> uid_address_map_;
    std::string cm_address_;
    uint64_t data_buffer_size_ = 0;
    std::shared_ptr<char> data_buffer_ = nullptr;
    std::shared_ptr<BufferMap> data_buffer_map_ = nullptr;

    std::shared_ptr<char> wi_buffer_ = nullptr;
    std::shared_ptr<BufferMap> wi_buffer_map_ = nullptr;
    std::shared_ptr<ConnectorInterface> node_connector_ = nullptr;
    std::atomic_uint64_t send_cnt_ = 0;
    std::atomic_uint64_t failed_remote_lock_cnt_ = 0;
    std::string node_listen_addr_;
    std::shared_ptr<WorkItem> wi_buffer_status_ = nullptr;
    std::shared_ptr<TsSink> ts_sink_ = nullptr;
    std::atomic_uint64_t recv_cnt_ = 0;
    std::atomic_uint64_t failed_self_locks_ = 0;

    void on_new_work_item(std::string address, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id);
    void on_node_connected(std::string address, uint64_t rem_group_id, uint64_t rem_node_id);
    void on_connection_refused(std::string address);
    void on_new_data (const std::string& /*address*/, uint64_t group_id, uint64_t node_id);

public:
    TsReceiver(
        uint64_t node_id,
        std::string listen_address,
        std::string output_uri,
        uint32_t timeslice_size,
        std::string central_manager_address,
        std::string monitoring_uri = ""
    );
};