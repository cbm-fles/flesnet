#include "CmFair.hpp"
#include "log.hpp"
#include <chrono>
#include <cstdint>
#include <exception>
#include <ostream>
#include <vector>

using namespace std;

CmFair::CmFair(std::string listen_address,
               uint64_t wi_buffer_size,
               uint64_t wi_buffer_map_size,
               std::string monitoring_uri,
               std::string hostname) : CentralManager(listen_address,
                                                      wi_buffer_size,
                                                      wi_buffer_map_size,
                                                      monitoring_uri,
                                                      hostname) {
  eval_worker_ = async(launch::async, &CmFair::eval_thread, this);
  periodic_log_ = async(launch::async, &CmFair::periodic_log, this);
}

void CmFair::on_new_node(uint64_t group_id, uint64_t node_id) {
  L_(debug) << "on_new_node start";
  unique_lock<mutex> l(mtx_);
  auto node_uid = MAKE_UID(group_id, node_id);
  node_status_map_[node_uid] = make_shared<NodeBufferStatus>();
  L_(debug) << "on_new_node end";
}

void CmFair::on_new_buffer_status(std::shared_ptr<char> wi_serialized, uint64_t group_id, uint64_t node_id) {
  L_(debug) << "on_new_buffer_status start";
  WiTimesliceStatus ts_status;
  ts_status.deserialize(wi_serialized);
  if (ts_status.sub_type == WiType::wi_timeslice_status) {
    if (group_id == SENDER_GROUP_ID) {
      L_(debug) << "Received buffer status of sender...";
      unique_lock<mutex> l(mtx_);
      sender_status_available_.push_back(ts_status);
      eval_worker_cv_.notify_all();
    } else if (group_id == RECEIVER_GROUP_ID) {
      L_(debug) << "Received buffer status of receiver ...";
      unique_lock<mutex> l(mtx_);
      receiver_status_available_.push_back(ts_status);
      eval_worker_cv_.notify_all();
    } else {
      L_(error) << "Received buffer status from unknown group ID: " << group_id;
    }
  } else if (ts_status.sub_type == WiType::wi_buffer_full_report) {
    L_(debug) << "Received buffer full report - node id: " << node_id;
    unique_lock<mutex> l(mtx_);
    buffer_full_reports_.push_back(ts_status);
    eval_worker_cv_.notify_all();
  } else {
    L_(error) << "WiTimesliceStatus unknown subtype: " << ts_status.sub_type << endl;
  }
  L_(debug) << "on_new_buffer_status end";
}

void CmFair::eval_node_status() {
  // We don't continuously lock, but interrupt it to not starve other threads.
  L_(debug) << "eval_node_status START";
  std::vector<uint64_t> processed_ts_indexes;
  {
    L_(debug) << "Processing reveiver status updates: ";
    unique_lock<mutex> l(mtx_);
    for (auto& wi : receiver_status_available_) {

      auto receiver_node_uid = MAKE_UID(wi.origin_group_id, wi.origin_node_id);
      auto receiver_status = node_status_map_[receiver_node_uid];
      L_(debug) << "\tReceiver Processed: " << endl
                << "\tNode ID: " << wi.origin_node_id << endl
                << "\tTS Count: " << wi.ts_indexes.size();
      for (auto& idx : wi.ts_indexes) {
        L_(debug) << "\tProcessed: " << idx;
      }
      ts_count_map[wi.origin_node_id] += wi.ts_indexes.size();
      if (receiver_status->vector_clock < wi.vector_clock) {
        receiver_status->buffer_fill_level = wi.buffer_level;
        receiver_status->full = false;
        receiver_status->vector_clock = wi.vector_clock;
      } else {
        L_(debug) << "\tRECEIVER status: Ignored status update";
      }
      processed_ts_indexes.insert(
          std::end(processed_ts_indexes),
          std::begin(wi.ts_indexes),
          std::end(wi.ts_indexes));
    }
    receiver_status_available_.clear();
  }

  {
    L_(debug) << "Processing buffer full reports: ";
    unique_lock<mutex> l(mtx_);
    for (auto& wi : buffer_full_reports_) {
      auto sender_node_uid = MAKE_UID(wi.origin_group_id, wi.origin_node_id);
      auto receiver_node_uid = wi.node_uids[0];
      auto receiver_status = node_status_map_[receiver_node_uid];
      auto sender_status = node_status_map_[sender_node_uid];

      L_(debug) << "\tBuffer Full Report: " << endl
                << "\tFrom Node ID: " << wi.origin_node_id << endl
                << "\tTo Node ID: " << NODE_ID(wi.node_uids[0]) << endl
                << "\tTS index: " << wi.ts_indexes[0];

      if (receiver_status->vector_clock < wi.vector_clock) {
        receiver_status->buffer_fill_level = wi.rem_buffer_level;
        receiver_status->full = true;
        receiver_status->vector_clock = wi.vector_clock;
      } else {
        L_(debug) << "\tBUFFER FULL status: Ignored status update";
      }
      sender_status->buffer_fill_level = wi.buffer_level;
      auto found_node = false;
      for (auto& send_status : ts_sending_status_by_age_) {
        auto& node_uid = send_status->sender_node_uid;
        auto& ts_index = send_status->ts_index;
        if (node_uid == sender_node_uid && ts_index == wi.ts_indexes[0]) {
          found_node = true;
          send_status->order_given = false;
          send_status->failed_sending_attempts++;
          nodes_load_[receiver_node_uid] -= send_status->ts_size;
          monitor_->QueueMetric("timeslice_forwarder_state",
                                {{"CM", "CM"},
                                 {"host", hostname_},
                                 {"receiver", to_string(NODE_ID(receiver_node_uid))}},
                                {{"cm_receiver_load", nodes_load_[receiver_node_uid]}});

          // L_(debug) << "\tBuffer full report processed: " << NODE_ID(node_uid);
          break;
        }
      }
      if (!found_node) {
        L_(warning) << "\tCould not found node to process buffer full repor";
      } else {
        L_(info) << "\tProcessed buffer full report";
      }
    }
    buffer_full_reports_.clear();
  }

  L_(debug) << "Updating ts sending queue: ";
  for (auto& ts_index : processed_ts_indexes) {
    auto found_idx = false;
    unique_lock<mutex> l(mtx_);
    for (auto it = ts_sending_status_by_age_.begin(); it != ts_sending_status_by_age_.end(); it++) {
      auto& idx = (*it)->ts_index;
      auto& receiver_uid = (*it)->receiver_node_uid;
      auto& ts_size = (*it)->ts_size;
      if (idx == ts_index) {
        nodes_load_[receiver_uid] -= ts_size;
        L_(debug) << "\tRemoved queue entry with TS idx: " << idx;
        ts_sending_status_by_age_.erase(it);
        found_idx = true;
        monitor_->QueueMetric("timeslice_forwarder_state",
                              {{"CM", "CM"},
                               {"host", hostname_},
                               {"receiver", to_string(NODE_ID(receiver_uid))}},
                              {{"cm_receiver_load", nodes_load_[receiver_uid]}});
        break;
      }
    }
    if (!found_idx) {
      L_(warning) << "\tCould not found ts index to removed it from queued timeslices";
    }
  }

  if (!processed_ts_indexes.empty()) {
    processed_timeslices_sum_ += processed_ts_indexes.size();
    *processed_timeslices_per_second_.value += processed_ts_indexes.size();
    monitor_->QueueMetric("timeslice_forwarder_state",
                          {{"CM", "CM"},
                           {"host", hostname_}},
                          {{"cm_processed_timeslices_sum", processed_timeslices_sum_}});
  }

  {
    L_(debug) << "Processing sender status: ";
    unique_lock<mutex> l(mtx_);
    for (auto& sender_status : sender_status_available_) {
      auto sender_uid = MAKE_UID(sender_status.origin_group_id, sender_status.origin_node_id);
      auto ts_idx = sender_status.ts_indexes[0];
      auto ts_size = sender_status.ts_sizes[0];
      auto sender_buffer_level = sender_status.buffer_level;
      L_(debug) << "\tStatus from:" << endl
                << "\tNode ID: " << NODE_ID(sender_uid) << endl
                << "\tTS Index: " << ts_idx;

      auto sending_status = make_shared<SendingStatus>();
      sending_status->sender_node_uid = sender_uid;
      sending_status->ts_index = ts_idx;
      sending_status->ts_size = ts_size;
      if (seen_ts_indexes.find(ts_idx) != seen_ts_indexes.end()) {
        L_(warning) << "\tSeen same index multiple times" << endl;
      }
      seen_ts_indexes.insert(ts_idx);
      node_status_map_[sender_uid]->buffer_fill_level = sender_buffer_level;
      ts_sending_status_by_age_.push_back(sending_status);
    }
    sender_status_available_.clear();
  }

  {
    unique_lock<mutex> l(mtx_);
    for (auto& ts_status : ts_sending_status_by_age_) {
      L_(debug) << "CALC FOR: " << NODE_ID(ts_status->sender_node_uid) << " - ts index: " << ts_status->ts_index;

      auto sender_uid = ts_status->sender_node_uid;
      if (ts_status->order_given) {
        L_(debug) << "\torder given: " << NODE_ID(ts_status->sender_node_uid) << " - ts index: " << ts_status->ts_index << " - ts_status->receiver: " << NODE_ID(ts_status->receiver_node_uid);
        continue;
      }

      auto connections = connection_manager_.get_connections(sender_uid);
      if (connections.empty()) { // node is not connected to any other node
        L_(debug) << "\tsender: " << NODE_ID(sender_uid) << " - no connections available";
        return;
      }

      // find receiver node with lowest load
      uint64_t node_uid_lowest_load;
      uint64_t lowest_load_value = UINT64_MAX;
      for (auto& conn : connections) {
        L_(debug) << "\tReceiver " << NODE_ID(conn) << " - full: " << node_status_map_[conn]->full << " - load: " << nodes_load_[conn];
        if (nodes_load_[conn] < lowest_load_value && !node_status_map_[conn]->full) {
          lowest_load_value = nodes_load_[conn];
          node_uid_lowest_load = conn;
          if (nodes_load_[conn] == 0) { // 0 is the lowest load value possible -> exit early
            break;
          }
        }
      }

      if (lowest_load_value == UINT64_MAX) {
        L_(debug) << "\tsender: " << NODE_ID(sender_uid) << " - lowest load exit";
        ts_status->order_given = false;
        continue;
      }

      L_(debug) << "\tEval node status: "
                << "Send from: " << NODE_ID(sender_uid) << " - to: " << NODE_ID(node_uid_lowest_load);
      nodes_load_[node_uid_lowest_load] += ts_status->ts_size;
      monitor_->QueueMetric("timeslice_forwarder_state",
                            {{"CM", "CM"},
                             {"host", hostname_},
                             {"receiver", to_string(NODE_ID(node_uid_lowest_load))}},
                            {{"cm_receiver_load", nodes_load_[node_uid_lowest_load]}});
      auto wi_tx = make_shared<WiTransmission>();
      wi_tx->node_uid = node_uid_lowest_load;
      wi_tx->type = WorkItem::transmission;
      ts_status->order_given = true;
      ts_status->receiver_node_uid = node_uid_lowest_load;
      int ret = Node::send_work_item(uid_address_map_[sender_uid], wi_tx);
      if (ret != 0) {
        L_(warning) << "\tfailed to send tranmission work item to " << NODE_ID(sender_uid);
      }
    }
  }
  L_(debug) << "eval_node_status END";
}

void CmFair::periodic_log() {
  while (!stop_eval_worker_) {
    this_thread::sleep_for(chrono::seconds(2));
    monitor_->QueueMetric("timeslice_forwarder_state",
                          {{"CM", "CM"},
                           {"host", hostname_}},
                          {{"cm_processed_timeslices_per_second", *processed_timeslices_per_second_.per_seconds()}});
  }
}

void CmFair::eval_thread() {
  while (!stop_eval_worker_) {
    {
      unique_lock<mutex> l(mtx_);
      auto wait_start = chrono::high_resolution_clock::now();
      eval_worker_cv_.wait_for(l, std::chrono::milliseconds(500), [&]() {
        return !sender_status_available_.empty() || !receiver_status_available_.empty() || !buffer_full_reports_.empty() || static_cast<bool>(stop_eval_worker_);
      });
      auto wait_time_ms = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - wait_start).count();
      monitor_->QueueMetric("timeslice_forwarder_state",
                            {{"CM", "CM"},
                             {"host", hostname_}},
                            {{"cm_idle_time_ms", wait_time_ms}});
    }
    auto eval_start = chrono::high_resolution_clock::now();
    eval_node_status();
    auto eval_time_ns = chrono::duration_cast<chrono::nanoseconds>(chrono::high_resolution_clock::now() - eval_start).count();
    monitor_->QueueMetric("timeslice_forwarder_state",
                          {{"CM", "CM"},
                           {"host", hostname_}},
                          {{"cm_evaluation_time_ns", eval_time_ns}});
  }
}

CmFair::~CmFair() {
  stop_eval_worker_ = true;
  try {
    eval_worker_.wait();
  } catch (exception e) {
  }
}
