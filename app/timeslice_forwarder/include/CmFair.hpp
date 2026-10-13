#pragma once
#include "CentralManager.hpp"
#include "WorkItems.hpp"
#include <cstdint>
#include <map>
#include <unordered_map>

class CmFair : public CentralManager {
public:
  CmFair(std::string listen_address,
         uint64_t wi_buffer_size,
         uint64_t wi_buffer_map_size,
         std::string monitoring_uri,
         std::string hostname);
  ~CmFair() override;

private:
  struct NodeBufferStatus {
    uint64_t vector_clock = 0;
    bool full = false;
    float buffer_fill_level = 0.0;
  };

  std::vector<WiTimesliceStatus> sender_status_available_;
  std::vector<WiTimesliceStatus> buffer_full_reports_;
  std::vector<WiTimesliceStatus> receiver_status_available_;

  // <node_uid, buffer status>
  std::unordered_map<uint64_t, std::shared_ptr<NodeBufferStatus>> node_status_map_;

  // <receiver node_uid, counter of concurrent receives>
  std::unordered_map<uint64_t, uint64_t> concurrent_receives_map_;

  std::set<uint64_t> seen_ts_indexes;

  // Node UID, TS count received
  std::unordered_map<uint64_t, uint64_t> ts_count_map;

  // tuple represents <Node UID, TS Index, Instruction given, failed attempts>
  struct SendingStatus {
    uint64_t sender_node_uid = 0;
    uint64_t ts_index = 0;
    uint64_t ts_size = 0;
    bool order_given = false;
    uint64_t failed_sending_attempts = 0;
    uint64_t receiver_node_uid = 0;
  };
  std::vector<std::shared_ptr<SendingStatus>> ts_sending_status_by_age_;

  std::condition_variable eval_worker_cv_;
  std::future<void> eval_worker_;
  std::future<void> periodic_log_;
  std::atomic_bool stop_eval_worker_ = false;

  void on_new_node(uint64_t group_id, uint64_t node_id) override;
  void on_new_buffer_status(std::shared_ptr<char> wi_serialized, uint64_t group_id, uint64_t node_id) override;
  void eval_node_status();
  void eval_thread();
  void periodic_log();
};