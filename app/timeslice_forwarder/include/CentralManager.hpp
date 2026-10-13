#pragma once
#include "LogVar.hpp"
#include "Monitor.hpp"
#include "WorkItems.hpp"
#include <atomic>
#include <cstdint>
#include <df/ConnectionManager.hpp>
#include <df/Connectors/ConnectorInfiniband.hpp>
#include <df/Node.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <df/WorkItems/WiTransmission.hpp>
#include <future>
#include <memory>

class CentralManager : public Node {
protected:
  /// @brief used to lock ANY shared resources
  std::mutex mtx_;
  std::shared_ptr<cbm::Monitor> monitor_{std::make_shared<cbm::Monitor>()};
  // maps used to translate node UID to IP addresses
  std::unordered_map<uint64_t, std::string> uid_address_map_;
  std::unordered_map<uint64_t, std::string> uid_listen_address_map_;

  // used to translate between node uid and its WI buffer map
  std::unordered_map<uint64_t, std::shared_ptr<BufferMap>> uid_buffer_map_map_;
  // std::shared_ptr<std::vector<uint64_t>> nodes_with_buffer_change_{std::make_shared<std::vector<uint64_t>>()};

  // WorkerThread worker;
  ConnectionManager connection_manager_; // used to take track of connections between nodes

  std::atomic_uint16_t target_idx_ = 0;
  std::atomic_uint64_t input_nodes_cnt_ = 0;
  std::atomic_uint64_t output_nodes_cnt_ = 0;
  std::string listen_address_;
  std::string hostname_;

  /**
   * @brief Will queue all sender node UID which want to get rid of Timeslices
   */
  std::vector<uint64_t> nodes_data_available_;

  /**
   * @brief key: receiver node UID, value: the number of assigned tasks
   * @details the value gets increased everytime the CM tells a sender to transmit its data to a receiver node.
   * The sender node will send a work item to the CM once it has processed a TS and the value for this receiver node will be decreased again.
   * In short, the node UID with the lowest value has currently the least amount of work.
   */
  std::unordered_map<uint64_t, std::atomic_uint64_t> nodes_load_;

  std::atomic_uint64_t processed_timeslices_sum_ = 0;
  Avg<std::atomic<double>> processed_timeslices_per_second_;

  /**
   * @brief (key: receiver node UID, value: is buffer full)
   * @details The central manager just puts out transmission commands. If the sender reports to the central manager that the buffer of the suggested
   * receiver node is full, it will be noted in this map by settings the node UID to true.
   * Once the receiver ndoe reports that it has finished some work, the value will be resetted to false.
   * If a node UID is set to true, this node will not be considered to transmit data to.
   */
  std::unordered_map<uint64_t, std::atomic_bool> nodes_buffer_full_;

  void on_node_connected(std::string address, uint64_t group_id, uint64_t node_id);

  void on_node_disconnected(std::string address, uint64_t group_id, uint64_t node_id);

  void on_new_work_item(std::string address, std::shared_ptr<char> wi_ptr, WorkItem::Type wi_type, uint64_t group_id, uint64_t node_id);

  void connect_nodes(uint64_t node_uid);

  virtual void on_new_buffer_status(std::shared_ptr<char> wi_serialized, uint64_t group_id, uint64_t node_id) = 0;
  virtual void on_new_node(uint64_t group_id, uint64_t node_id) = 0;

public:
  constexpr static uint64_t SENDER_GROUP_ID = 1;
  constexpr static uint64_t RECEIVER_GROUP_ID = 2;
  CentralManager() = delete;
  CentralManager(
      std::string listen_address,
      uint64_t wi_buffer_size,
      uint64_t wi_buffer_map_size,
      std::string monitoring_uri = "",
      std::string hostname = "");
  virtual ~CentralManager() {};
};