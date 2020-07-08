// Copyright 2018 Farouk Salem <salem@zib.de>

#include "InputTimesliceManager.hpp"

// TO BE REMOVED
#include <fstream>
#include <iomanip>

namespace tl_libfabric {

InputTimesliceManager::InputTimesliceManager(uint32_t scheduler_index,
                                             uint32_t compute_conn_count,
                                             uint32_t interval_length,
                                             uint64_t data_source_desc,
                                             uint64_t desc_length,
                                             uint64_t start_index_desc,
                                             uint64_t timeslice_size,
                                             std::string log_directory,
                                             bool enable_logging)
    : compute_count_(compute_conn_count),
      virtual_compute_count_(compute_conn_count),
      scheduler_index_(scheduler_index), interval_length_(interval_length),
      data_source_desc_(data_source_desc), desc_length_(desc_length),
      start_index_desc_(start_index_desc), timeslice_size_(timeslice_size),
      log_directory_(log_directory), enable_logging_(enable_logging) {

  last_conn_desc_.resize(compute_count_, 0);
  last_conn_timeslice_.resize(compute_count_, ConstVariables::MINUS_ONE);
  virtual_physical_compute_mapping_.resize(compute_count_);
  for (uint32_t i = 0; i < compute_count_; ++i) {
    conn_timeslice_info_.add(i, new SizedMap<uint64_t, TimesliceInfo*>());
    conn_desc_timeslice_info_.add(i, new SizedMap<uint64_t, uint64_t>());
    future_conn_timeslices_.add(i, new std::set<uint64_t>());
    virtual_physical_compute_mapping_[i] = i;
  }
  refill_future_timeslices(interval_length_);
}

void InputTimesliceManager::refill_future_timeslices(uint64_t up_to_timeslice) {
  if (next_start_future_timeslice_ >= up_to_timeslice)
    return;
  uint32_t comp_index = next_start_future_timeslice_ % virtual_compute_count_;
  for (uint64_t ts = next_start_future_timeslice_; ts < up_to_timeslice; ++ts) {
    while (redistribution_decisions_log_.contains(
        virtual_physical_compute_mapping_[comp_index]))
      comp_index = (comp_index + 1) % virtual_compute_count_;
    future_conn_timeslices_.get(virtual_physical_compute_mapping_[comp_index])
        ->insert(ts);
    comp_index = (comp_index + 1) % virtual_compute_count_;
  }
  next_start_future_timeslice_ = up_to_timeslice;
}

void InputTimesliceManager::check_to_add_rescheduled_timeslices(
    uint32_t compute_index) {
  if (to_be_moved_timeslices_.empty())
    return;
  // TODO check the correctness of the new desc(s)
  SizedMap<uint64_t, std::vector<std::set<uint64_t>*>>::iterator it =
      to_be_moved_timeslices_.get_begin_iterator();
  std::set<uint64_t>* rescheduled_timeslices;
  while (it != to_be_moved_timeslices_.get_end_iterator()) {
    L_(debug) << "[" << compute_index << "]check to add: trigger " << it->first
              << " next " << get_connection_next_timeslice(compute_index)
              << " list " << it->second[compute_index];
    if (get_connection_next_timeslice(compute_index) > it->first &&
        it->second[compute_index] != nullptr &&
        !it->second[compute_index]->empty()) {
      L_(debug) << "adding rescheduled timeslices after " << it->first << " to "
                << compute_index << " ... last transmitted is "
                << conn_timeslice_info_.get(compute_index)->get_last_key();
      rescheduled_timeslices = it->second[compute_index];
      assert(rescheduled_timeslices != nullptr);
      std::set<uint64_t>::iterator set_it = rescheduled_timeslices->begin();
      while (set_it != rescheduled_timeslices->end()) {
        future_conn_timeslices_.get(compute_index)->insert(*set_it);
        ++set_it;
      }
      delete rescheduled_timeslices;
      it->second[compute_index] = rescheduled_timeslices = nullptr;
    }
    ++it;
  }
  // TODO to be written in a better way
  while (!to_be_moved_timeslices_.empty()) {
    it = to_be_moved_timeslices_.get_begin_iterator();
    bool all_empty = true;
    for (uint32_t i = 0; i < compute_count_; i++) {
      if (it->second[i] != nullptr && !it->second[i]->empty()) {
        all_empty = false;
        break;
      }
    }
    if (all_empty) {
      assert(to_be_moved_timeslices_.remove(it->first));
    } else {
      break;
    }
  }
}

std::vector<uint64_t>
InputTimesliceManager::undo_transmitted_timeslices_after_trigger(
    uint64_t timeslice_trigger) {
  std::vector<uint64_t> undo_timeslices;
  if (timeslice_trigger > next_start_future_timeslice_)
    return undo_timeslices;

  SizedMap<uint64_t, TimesliceInfo*>* timeslices_map;
  TimesliceInfo* last_timeslice_info;
  std::set<uint64_t>* future_timeslices;
  for (uint32_t conn = 0; conn < compute_count_; ++conn) {
    timeslices_map = conn_timeslice_info_.get(conn);
    uint64_t last_timeslice;
    while (!timeslices_map->empty()) {
      last_timeslice = timeslices_map->get_last_key();
      if (last_timeslice <= timeslice_trigger)
        break;
      last_timeslice_info = timeslices_map->get(last_timeslice);
      assert(last_timeslice_info->completion_acked_duration == 0);
      assert(last_timeslice_info->compute_desc == last_conn_desc_[conn]);
      assert(last_timeslice == last_conn_timeslice_[conn]);
      // remove conn_desc_timeslices
      assert(conn_desc_timeslice_info_.get(conn)->remove(
          last_timeslice_info->compute_desc));
      // remove conn_timeslice_info
      assert(timeslices_map->remove(last_timeslice));
      undo_timeslices.push_back(last_timeslice);
      L_(debug) << "Removing " << last_timeslice << " from " << conn;
      // update the last_conn_desc
      last_conn_desc_[conn]--;
      // update last_conn_timeslice
      last_conn_timeslice_[conn] = timeslices_map->get_last_key();
    }
    // remove future_timeslices
    future_timeslices = future_conn_timeslices_.get(conn);
    while (!future_timeslices->empty()) {
      last_timeslice = *(--future_timeslices->end());
      if (last_timeslice <= timeslice_trigger) {
        L_(debug) << "last_timeslice of " << conn << " is " << last_timeslice;
        break;
      }
      future_timeslices->erase(last_timeslice);
    }
  }
  timeslices_map = nullptr;
  last_timeslice_info = nullptr;
  future_timeslices = nullptr;
  return undo_timeslices;
}

// PUBLIC
InputTimesliceManager*
InputTimesliceManager::get_instance(uint32_t scheduler_index,
                                    uint32_t compute_conn_count,
                                    uint32_t interval_length,
                                    uint64_t data_source_desc,
                                    uint64_t desc_length,
                                    uint64_t start_index_desc,
                                    uint64_t timeslice_size,
                                    std::string log_directory,
                                    bool enable_logging) {
  if (instance_ == nullptr) {
    instance_ = new InputTimesliceManager(
        scheduler_index, compute_conn_count, interval_length, data_source_desc,
        desc_length, start_index_desc, timeslice_size, log_directory,
        enable_logging);
  }
  return instance_;
}

InputTimesliceManager* InputTimesliceManager::get_instance() {
  assert(instance_ != nullptr);
  return instance_;
}

uint64_t
InputTimesliceManager::get_connection_next_timeslice(uint32_t compute_index) {
  if (future_conn_timeslices_.get(compute_index)->empty() &&
      std::find(virtual_physical_compute_mapping_.begin(),
                virtual_physical_compute_mapping_.end(),
                compute_index) == virtual_physical_compute_mapping_.end()) {
    return ConstVariables::MINUS_ONE;
  }
  if (future_conn_timeslices_.get(compute_index)->empty()) {
    refill_future_timeslices(next_start_future_timeslice_ + interval_length_);
  }
  return (*future_conn_timeslices_.get(compute_index)->begin());
}

void InputTimesliceManager::log_timeslice_transmit_time(uint32_t compute_index,
                                                        uint64_t timeslice,
                                                        uint64_t size) {
  assert(!conn_timeslice_info_.get(compute_index)->contains(timeslice));

  uint64_t descriptor_index = last_conn_desc_[compute_index] + 1,
           timeslice_data =
               size +
               (last_conn_timeslice_[compute_index] == ConstVariables::MINUS_ONE
                    ? 0
                    : conn_timeslice_info_.get(compute_index)
                          ->get(last_conn_timeslice_[compute_index])
                          ->data);

  TimesliceInfo* timeslice_info = new TimesliceInfo();
  timeslice_info->data = timeslice_data;
  timeslice_info->compute_desc = descriptor_index;
  timeslice_info->transmit_time = std::chrono::high_resolution_clock::now();
  assert(
      conn_timeslice_info_.get(compute_index)->add(timeslice, timeslice_info));
  assert(conn_desc_timeslice_info_.get(compute_index)
             ->add(descriptor_index, timeslice));
  assert(future_conn_timeslices_.get(compute_index)->erase(timeslice) == 1);
  ++last_conn_desc_[compute_index];
  last_conn_timeslice_[compute_index] = timeslice;
  ++sent_timeslices_;

  check_to_add_rescheduled_timeslices(compute_index);
}

bool InputTimesliceManager::acknowledge_timeslice_rdma_write(
    uint32_t compute_index, uint64_t timeslice) {
  if (!conn_timeslice_info_.get(compute_index)->contains(timeslice)) {
    L_(warning) << "[i_" << scheduler_index_ << "][ACK_RDMA_WRITE] ts "
                << timeslice << " does not belong to conn_" << compute_index;
    return false;
  }
  TimesliceInfo* timeslice_info =
      conn_timeslice_info_.get(compute_index)->get(timeslice);
  if (timeslice_info->rdma_acked_duration != 0) {
    timeslice_info = nullptr;
    return false;
  }
  timeslice_info->rdma_acked_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now() -
          timeslice_info->transmit_time)
          .count();
  timeslice_info = nullptr;
  return true;
}

double InputTimesliceManager::acknowledge_timeslices_completion(
    uint32_t compute_index, uint64_t up_to_descriptor_id) {
  uint64_t sum_latency = 0;
  uint32_t count = 0;
  SizedMap<uint64_t, uint64_t>* desc_timeslice_list =
      conn_desc_timeslice_info_.get(compute_index);
  assert(desc_timeslice_list != nullptr);

  SizedMap<uint64_t, uint64_t>::iterator transmitted_timeslice_iterator =
      desc_timeslice_list->get_begin_iterator();
  TimesliceInfo* timeslice_info;
  while (transmitted_timeslice_iterator !=
             desc_timeslice_list->get_end_iterator() &&
         transmitted_timeslice_iterator->first <= up_to_descriptor_id) {
    timeslice_info = conn_timeslice_info_.get(compute_index)
                         ->get(transmitted_timeslice_iterator->second);
    assert(timeslice_info != nullptr);
    timeslice_info->completion_acked_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() -
            timeslice_info->transmit_time)
            .count();

    // Calculate the latency
    sum_latency += timeslice_info->completion_acked_duration -
                   timeslice_info->rdma_acked_duration;
    ++count;

    assert(
        desc_timeslice_list->contains(transmitted_timeslice_iterator->first));
    // assert(desc_timeslice_list->remove(transmitted_timeslice_iterator->first));
    assert(desc_timeslice_list->remove(transmitted_timeslice_iterator));
    transmitted_timeslice_iterator =
        conn_desc_timeslice_info_.get(compute_index)->get_begin_iterator();
  }
  return count == 0 ? 0 : sum_latency / count;
}

bool InputTimesliceManager::is_timeslice_rdma_acked(uint32_t compute_index,
                                                    uint64_t timeslice) {
  if (!conn_timeslice_info_.get(compute_index)->contains(timeslice)) {
    L_(warning) << "[i_" << scheduler_index_ << "] [RDMA ACKED] ts "
                << timeslice << " does not belong to conn_" << compute_index;
    return true;
  }

  TimesliceInfo* timeslice_info =
      conn_timeslice_info_.get(compute_index)->get(timeslice);
  if (timeslice_info->rdma_acked_duration != 0 ||
      timeslice_info->completion_acked_duration != 0) {
    timeslice_info = nullptr;
    return true;
  }
  return false;
}

bool InputTimesliceManager::is_timeslice_belongs_to_timeout_connection(
    uint64_t timeslice, const std::set<uint32_t> timeout_connections) {
  if (timeslice >= next_start_future_timeslice_) {
    refill_future_timeslices(timeslice + 1);
  }
  std::set<uint32_t>::iterator it = timeout_connections.begin();
  std::set<uint64_t>* future_ts;
  while (it != timeout_connections.end()) {
    future_ts = future_conn_timeslices_.get(*it);
    if (future_ts->find(timeslice) != future_ts->end() ||
        conn_timeslice_info_.get(*it)->contains(timeslice)) {
      future_ts = nullptr;
      return true;
    }
    ++it;
  }
  future_ts = nullptr;
  return false;
}

uint32_t InputTimesliceManager::get_compute_connection_count() {
  return compute_count_;
}

uint64_t
InputTimesliceManager::get_last_acked_descriptor(uint32_t compute_index) {
  if (conn_desc_timeslice_info_.get(compute_index)->empty()) {
    if (conn_timeslice_info_.get(compute_index)->empty())
      return 0;
    return conn_timeslice_info_.get(compute_index)
        ->get(conn_timeslice_info_.get(compute_index)->get_last_key())
        ->compute_desc;
  }
  return conn_desc_timeslice_info_.get(compute_index)
             ->get_begin_iterator()
             ->first -
         1;
}

uint64_t
InputTimesliceManager::get_timeslice_by_descriptor(uint32_t compute_index,
                                                   uint64_t descriptor) {
  if (conn_desc_timeslice_info_.get(compute_index)->contains(descriptor))
    return conn_desc_timeslice_info_.get(compute_index)->get(descriptor);

  uint64_t timeslice = ConstVariables::MINUS_ONE;

  SizedMap<uint64_t, TimesliceInfo*>* timeslice_info_list =
      conn_timeslice_info_.get(compute_index);
  SizedMap<uint64_t, TimesliceInfo*>::iterator timeslice_info_it =
      timeslice_info_list->get_end_iterator();
  do {
    --timeslice_info_it;
    if (timeslice_info_it->second->compute_desc == descriptor) {
      timeslice = timeslice_info_it->first;
      break;
    }
  } while (timeslice_info_it != timeslice_info_list->get_begin_iterator());

  timeslice_info_list = nullptr;
  return timeslice;
}

uint64_t
InputTimesliceManager::get_last_rdma_acked_timeslice(uint32_t compute_index) {
  SizedMap<uint64_t, uint64_t>* incompleted_desc_timeslices =
      conn_desc_timeslice_info_.get(compute_index);
  if (incompleted_desc_timeslices->empty()) {
    return last_conn_timeslice_[compute_index];
  }

  uint64_t timeslice = ConstVariables::MINUS_ONE;

  SizedMap<uint64_t, uint64_t>::iterator incompleted_it =
      incompleted_desc_timeslices->get_end_iterator();
  SizedMap<uint64_t, TimesliceInfo*>* conn_timeslice_list =
      conn_timeslice_info_.get(compute_index);
  TimesliceInfo* timeslice_info;
  do {
    --incompleted_it;
    timeslice_info = conn_timeslice_list->get(incompleted_it->second);
    if (timeslice_info->rdma_acked_duration != 0) {
      timeslice = incompleted_it->second;
      break;
    }

  } while (incompleted_it != incompleted_desc_timeslices->get_begin_iterator());

  // When all the timeslices in conn_desc_timeslice_info_ are not RDMA acked
  // yet, return the last one not in the conn_desc_timeslice_info_
  if (timeslice == ConstVariables::MINUS_ONE) {
    timeslice =
        get_timeslice_by_descriptor(compute_index, incompleted_it->first - 1);
  }
  return timeslice;
}

uint64_t InputTimesliceManager::get_last_timeslice_before_blockage(
    const uint32_t failed_compute_index,
    const std::set<uint32_t> timeout_connections,
    const uint64_t current_interval_start_ts,
    const uint64_t current_interval_end_ts) {

  uint64_t timeslice =
      (data_source_desc_ - desc_length_ - start_index_desc_) / timeslice_size_;

  while (!timeout_connections.empty() &&
         is_timeslice_belongs_to_timeout_connection(timeslice,
                                                    timeout_connections)) {
    --timeslice;
  }

  uint64_t failed_compute_timeslices =
      count_unacked_timeslices_of_interval(failed_compute_index,
                                           current_interval_start_ts,
                                           current_interval_end_ts) +
      count_future_timeslices_of_interval(failed_compute_index,
                                          current_interval_start_ts,
                                          current_interval_end_ts);
  if (failed_compute_timeslices == 0) {
    return timeslice;
  }
  return std::min(timeslice, current_interval_end_ts);
}

uint64_t InputTimesliceManager::get_last_connection_descriptor_index(
    uint32_t compute_index) {
  assert(compute_index < last_conn_desc_.size());
  return last_conn_desc_[compute_index];
}

uint64_t InputTimesliceManager::count_timeslices_of_interval(
    uint32_t compute_index, uint64_t start_ts, uint64_t end_ts) {
  uint64_t count = 0;
  SizedMap<uint64_t, TimesliceInfo*>* timesliceInfos =
      conn_timeslice_info_.get(compute_index);
  if (timesliceInfos->empty()) {
    timesliceInfos = nullptr;
    return count;
  }

  SizedMap<uint64_t, TimesliceInfo*>::iterator timesliceInfo_it =
      timesliceInfos->get_end_iterator();
  do {
    --timesliceInfo_it;
    if (timesliceInfo_it->first >= start_ts &&
        timesliceInfo_it->first <= end_ts)
      ++count;
  } while (timesliceInfo_it != timesliceInfos->get_begin_iterator());
  timesliceInfos = nullptr;
  return count;
}

uint64_t InputTimesliceManager::count_unacked_timeslices_of_interval(
    uint32_t compute_index, uint64_t start_ts, uint64_t end_ts) {
  refill_future_timeslices(end_ts + 1);
  uint64_t count = 0;
  SizedMap<uint64_t, uint64_t>* desc_timeslices =
      conn_desc_timeslice_info_.get(compute_index);
  if (desc_timeslices->empty()) {
    return 0;
  }

  if (desc_timeslices->get_begin_iterator()->second >= start_ts &&
      (--desc_timeslices->get_end_iterator())->second <= end_ts)
    return desc_timeslices->size();

  SizedMap<uint64_t, uint64_t>::iterator desc_timeslice_it =
      desc_timeslices->get_end_iterator();
  do {
    --desc_timeslice_it;
    if (desc_timeslice_it->second >= start_ts &&
        desc_timeslice_it->second <= end_ts)
      ++count;
  } while (desc_timeslice_it != desc_timeslices->get_begin_iterator());

  return count;
}

uint64_t InputTimesliceManager::count_future_timeslices_of_interval(
    uint32_t compute_index, uint64_t start_ts, uint64_t end_ts) {
  refill_future_timeslices(end_ts + 1);
  uint64_t count = 0;
  std::set<uint64_t> future_timeslices =
      (*future_conn_timeslices_.get(compute_index));
  if (future_timeslices.empty())
    return 0;

  if (*future_timeslices.begin() >= start_ts &&
      *(--future_timeslices.end()) <= end_ts)
    return future_timeslices.size();

  for (uint64_t ts : future_timeslices) {
    if (ts >= start_ts && ts <= end_ts)
      ++count;
  }
  return count;
}

std::pair<uint64_t, uint64_t>
InputTimesliceManager::get_data_and_desc_of_timeslice(uint32_t compute_index,
                                                      uint32_t timeslice) {
  if (!conn_timeslice_info_.get(compute_index)->contains(timeslice)) {
    L_(fatal) << "compute index " << compute_index << " timeslice " << timeslice
              << " last conn_timeslice " << last_conn_timeslice_[compute_index]
              << " conn_timeslice_info size "
              << conn_timeslice_info_.get(compute_index)->size();
    assert(false);
  }
  TimesliceInfo* timeslice_info =
      conn_timeslice_info_.get(compute_index)->get(timeslice);
  std::pair<uint64_t, uint64_t> info =
      std::make_pair(timeslice_info->data, timeslice_info->compute_desc);

  timeslice_info = nullptr;
  return info;
}

std::pair<uint64_t, uint64_t>
InputTimesliceManager::get_data_and_desc_of_last_timeslice(
    uint32_t compute_index) {
  if (last_conn_timeslice_[compute_index] == ConstVariables::MINUS_ONE)
    return std::pair<uint64_t, uint64_t>(0, 0);
  return get_data_and_desc_of_timeslice(compute_index,
                                        last_conn_timeslice_[compute_index]);
}

std::vector<uint64_t> InputTimesliceManager::consider_reschedule_decision(
    HeartbeatFailedNodeInfo failed_node_info,
    const std::set<uint32_t> timeout_connections) {
  assert(compute_count_ > timeout_connections.size());
  std::vector<uint64_t> undo_timeslices =
      manage_after_trigger_timeslices_on_failure(failed_node_info);
  std::vector<uint64_t> failed_timeslices =
      retrieve_failed_timeslices_on_failure(failed_node_info);
  undo_timeslices.insert(undo_timeslices.end(), failed_timeslices.begin(),
                         failed_timeslices.end());

  distribute_failed_timeslices_on_active_connections(failed_node_info,
                                                     timeout_connections);

  redistribution_decisions_log_.add(failed_node_info.index,
                                    failed_node_info.timeslice_trigger);
  for (uint32_t i = 0; i < compute_count_; ++i) {
    L_(debug) << "last transmitted ts of " << i << " is "
              << last_conn_timeslice_[i] << " and desc " << last_conn_desc_[i];
    check_to_add_rescheduled_timeslices(i);
  }

  sent_timeslices_ -= undo_timeslices.size();
  return undo_timeslices;
}

std::vector<uint64_t>
InputTimesliceManager::manage_after_trigger_timeslices_on_failure(
    HeartbeatFailedNodeInfo failed_node_info) {
  std::vector<uint64_t> undo_timeslices;
  if (next_start_future_timeslice_ <=
      (failed_node_info.timeslice_trigger + 1)) {
    // Refill the future timeslices until the trigger to have all timeslices
    // that should be distributed over other compute nodes
    refill_future_timeslices(failed_node_info.timeslice_trigger + 1);
  } else {
    // Check if timeslice_trigger is already passed!! (return them back to
    // the queue for the correct ordering)
    std::vector<uint64_t> list = undo_transmitted_timeslices_after_trigger(
        failed_node_info.timeslice_trigger);
    undo_timeslices.insert(undo_timeslices.end(), list.begin(), list.end());
    L_(debug) << "[manage_future_timeslice_on_failure]["
              << failed_node_info.index << "] undo after trigger "
              << undo_timeslices.size();
    // update the desc correspondingly
    next_start_future_timeslice_ = failed_node_info.timeslice_trigger + 1;
  }
  return undo_timeslices;
}

std::vector<uint64_t>
InputTimesliceManager::retrieve_failed_timeslices_on_failure(
    HeartbeatFailedNodeInfo failed_node_info) {
  std::vector<uint64_t> undo_timeslices;
  std::set<uint64_t>* failed_timeslice =
      future_conn_timeslices_.get(failed_node_info.index);
  // clean the conn_desc_timeslice_info_ and move them back to future
  // timeslices
  SizedMap<uint64_t, uint64_t>* failed_conn_desc_timeslice_info =
      conn_desc_timeslice_info_.get(failed_node_info.index);
  assert(failed_conn_desc_timeslice_info->empty() ||
         failed_conn_desc_timeslice_info->get_begin_iterator()->first ==
             failed_node_info.last_completed_desc + 1);
  while (!failed_conn_desc_timeslice_info->empty()) {
    failed_timeslice->insert(
        failed_conn_desc_timeslice_info->get_begin_iterator()->second);
    undo_timeslices.push_back(
        failed_conn_desc_timeslice_info->get_begin_iterator()->second);
    assert(failed_conn_desc_timeslice_info->remove(
        failed_conn_desc_timeslice_info->get_begin_iterator()->first));
  }
  return undo_timeslices;
}

void InputTimesliceManager::distribute_failed_timeslices_on_active_connections(
    HeartbeatFailedNodeInfo failed_node_info,
    const std::set<uint32_t> timeout_connections) {

  std::set<uint64_t>* failed_timeslice =
      future_conn_timeslices_.get(failed_node_info.index);
  // Distribute failed_timeslices over active connections in a temporary array
  if (!failed_timeslice->empty()) {
    std::vector<std::set<uint64_t>*> movable_ts(compute_count_, nullptr);
    uint32_t compute_index = 0;
    while (!failed_timeslice->empty()) {
      while (timeout_connections.find(compute_index) !=
             timeout_connections.end())
        compute_index = (compute_index + 1) % compute_count_;
      if (movable_ts[compute_index] == nullptr)
        movable_ts[compute_index] = new std::set<uint64_t>();
      movable_ts[compute_index]->insert((*failed_timeslice->begin()));
      // L_(info) << "ts "<< (*failed_timeslice->begin()) << " of CN " <<
      // failed_node_info.index << " moved to " << compute_index;
      failed_timeslice->erase((*failed_timeslice->begin()));
      compute_index = (compute_index + 1) % compute_count_;
    }
    to_be_moved_timeslices_.add(failed_node_info.timeslice_trigger, movable_ts);
  }
  failed_timeslice = nullptr;
}

void InputTimesliceManager::generate_log_files() {
  if (!enable_logging_)
    return;

  /////////////////////////////////////////////////////////////////
  std::ofstream blocked_duration_log_file;
  blocked_duration_log_file.open(log_directory_ + "/" +
                                 std::to_string(scheduler_index_) +
                                 ".input.ts_blocked_duration.out");

  blocked_duration_log_file << std::setw(25) << "Timeslice" << std::setw(25)
                            << "Compute Index" << std::setw(25) << "IB"
                            << std::setw(25) << "CB" << std::setw(25) << "MR"
                            << "\n";

  uint64_t last_ts = std::max(
      (timeslice_IB_blocked_duration_log_.empty()
           ? 0
           : timeslice_IB_blocked_duration_log_.get_last_key()),
      std::max((timeslice_CB_blocked_duration_log_.empty()
                    ? 0
                    : timeslice_CB_blocked_duration_log_.get_last_key()),
               (timeslice_MR_blocked_duration_log_.empty()
                    ? 0
                    : timeslice_MR_blocked_duration_log_.get_last_key())));
  for (uint64_t ts = 0; ts <= last_ts; ts++) {
    blocked_duration_log_file
        << std::setw(25) << ts << std::setw(25) << (ts % compute_count_)
        << std::setw(25)
        << (timeslice_IB_blocked_duration_log_.contains(ts)
                ? timeslice_IB_blocked_duration_log_.get(ts) / 1000.0
                : 0)
        << std::setw(25)
        << (timeslice_CB_blocked_duration_log_.contains(ts)
                ? timeslice_CB_blocked_duration_log_.get(ts) / 1000.0
                : 0)
        << std::setw(25)
        << (timeslice_MR_blocked_duration_log_.contains(ts)
                ? timeslice_MR_blocked_duration_log_.get(ts) / 1000.0
                : 0)
        << "\n";
  }

  blocked_duration_log_file.flush();
  blocked_duration_log_file.close();

  /////////////////////////////////////////////////////////////////

  std::ofstream block_log_file;
  block_log_file.open(log_directory_ + "/" + std::to_string(scheduler_index_) +
                      ".input.ts_info.out");

  block_log_file << std::setw(25) << "Timeslice" << std::setw(25)
                 << "Compute Index" << std::setw(25) << "RDMA ACK duration"
                 << std::setw(25) << "Completion ACK duration"
                 << "\n";

  for (uint32_t i = 0; i < conn_timeslice_info_.size(); ++i) {
    SizedMap<uint64_t, TimesliceInfo*>::iterator delaying_time =
        conn_timeslice_info_.get(i)->get_begin_iterator();
    while (delaying_time != conn_timeslice_info_.get(i)->get_end_iterator()) {
      block_log_file << std::setw(25) << delaying_time->first << std::setw(25)
                     << i << std::setw(25)
                     << delaying_time->second->rdma_acked_duration
                     << std::setw(25)
                     << delaying_time->second->completion_acked_duration
                     << "\n";
      delaying_time++;
    }
  }
  block_log_file.flush();
  block_log_file.close();
}

uint64_t InputTimesliceManager::log_timeslice_IB_blocked(uint64_t timeslice,
                                                         bool sent_completed) {
  uint64_t duration = ConstVariables::ZERO;
  if (sent_completed) {
    if (timeslice_IB_blocked_start_log_.contains(timeslice)) {
      duration = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::high_resolution_clock::now() -
                     timeslice_IB_blocked_start_log_.get(timeslice))
                     .count();
      timeslice_IB_blocked_duration_log_.add(timeslice, duration);
      timeslice_IB_blocked_start_log_.remove(timeslice);
    }
  } else {
    timeslice_IB_blocked_start_log_.add(
        timeslice, std::chrono::high_resolution_clock::now());
  }
  return duration;
}

uint64_t InputTimesliceManager::log_timeslice_CB_blocked(uint64_t timeslice,
                                                         bool sent_completed) {
  uint64_t duration = ConstVariables::ZERO;
  if (sent_completed) {
    if (timeslice_CB_blocked_start_log_.contains(timeslice)) {
      duration = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::high_resolution_clock::now() -
                     timeslice_CB_blocked_start_log_.get(timeslice))
                     .count();
      timeslice_CB_blocked_duration_log_.add(timeslice, duration);
      timeslice_CB_blocked_start_log_.remove(timeslice);
    }
  } else {
    timeslice_CB_blocked_start_log_.add(
        timeslice, std::chrono::high_resolution_clock::now());
  }
  return duration;
}

uint64_t InputTimesliceManager::log_timeslice_MR_blocked(uint64_t timeslice,
                                                         bool sent_completed) {
  uint64_t duration = ConstVariables::ZERO;
  if (sent_completed) {
    if (timeslice_MR_blocked_start_log_.contains(timeslice)) {
      duration = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::high_resolution_clock::now() -
                     timeslice_MR_blocked_start_log_.get(timeslice))
                     .count();
      timeslice_MR_blocked_duration_log_.add(timeslice, duration);
      timeslice_MR_blocked_start_log_.remove(timeslice);
    }
  } else {
    timeslice_MR_blocked_start_log_.add(
        timeslice, std::chrono::high_resolution_clock::now());
  }
  return duration;
}

InputTimesliceManager* InputTimesliceManager::instance_ = nullptr;
} // namespace tl_libfabric
