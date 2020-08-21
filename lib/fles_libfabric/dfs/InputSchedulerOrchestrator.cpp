// Copyright 2019 Farouk Salem <salem@zib.de>

#include "InputSchedulerOrchestrator.hpp"

namespace tl_libfabric {

//// Common Methods
void InputSchedulerOrchestrator::initialize(uint32_t scheduler_index,
                                            uint32_t compute_conn_count,
                                            uint64_t init_heartbeat_timeout,
                                            uint32_t timeout_history_size,
                                            uint32_t timeout_factor,
                                            uint32_t inactive_factor,
                                            uint32_t inactive_retry_count,
                                            uint32_t interval_length,
                                            uint64_t data_source_desc,
                                            uint64_t desc_length,
                                            uint64_t start_index_desc,
                                            uint64_t timeslice_size,
                                            std::string log_directory,
                                            bool enable_logging) {
  interval_scheduler_ = InputIntervalScheduler::get_instance(
      scheduler_index, compute_conn_count, interval_length, log_directory,
      enable_logging);
  timeslice_manager_ = InputTimesliceManager::get_instance(
      scheduler_index, compute_conn_count, interval_length, data_source_desc,
      desc_length, start_index_desc, timeslice_size, log_directory,
      enable_logging);
  heartbeat_manager_ = InputHeartbeatManager::get_instance(
      scheduler_index, compute_conn_count, init_heartbeat_timeout,
      timeout_history_size, timeout_factor, inactive_factor,
      inactive_retry_count, log_directory, enable_logging);
  SchedulerOrchestrator::initialize(heartbeat_manager_);
}

void InputSchedulerOrchestrator::update_compute_connection_count(
    uint32_t compute_count) {
  // TODO remove
  assert(false);
  interval_scheduler_->update_compute_connection_count(compute_count);
}

void InputSchedulerOrchestrator::update_input_begin_time(
    std::chrono::high_resolution_clock::time_point begin_time) {
  interval_scheduler_->update_input_begin_time(begin_time);
}

uint32_t InputSchedulerOrchestrator::get_compute_connection_count() {
  return interval_scheduler_->get_compute_connection_count();
}

void InputSchedulerOrchestrator::generate_log_files() {
  interval_scheduler_->generate_log_files();
  timeslice_manager_->generate_log_files();
}

//// InputIntervalScheduler Methods
void InputSchedulerOrchestrator::add_proposed_meta_data(
    const IntervalMetaData meta_data) {
  interval_scheduler_->add_proposed_meta_data(meta_data);
}

const IntervalMetaData*
InputSchedulerOrchestrator::get_actual_meta_data(uint64_t interval_index) {
  return interval_scheduler_->get_actual_meta_data(interval_index);
}

uint64_t InputSchedulerOrchestrator::get_last_timeslice_to_send() {
  return interval_scheduler_->get_last_timeslice_to_send();
}

int64_t InputSchedulerOrchestrator::get_next_fire_time() {
  return interval_scheduler_->get_next_fire_time();
}

//// InputTimesliceManager Methods

uint64_t InputSchedulerOrchestrator::get_connection_next_timeslice(
    uint32_t compute_index) {
  if (is_connection_timed_out(compute_index))
    return ConstVariables::MINUS_ONE;

  uint64_t timeslice_trigger =
      heartbeat_manager_->get_timeslice_blockage_trigger();
  uint64_t next =
      timeslice_manager_->get_connection_next_timeslice(compute_index);
  if (next == ConstVariables::MINUS_ONE ||
      (timeslice_trigger != ConstVariables::MINUS_ONE &&
       next >= timeslice_trigger))
    return ConstVariables::MINUS_ONE;
  return next;
}

void InputSchedulerOrchestrator::mark_timeslice_transmitted(
    uint32_t compute_index, uint64_t timeslice, uint64_t size) {
  interval_scheduler_->increament_sent_timeslices(timeslice);
  timeslice_manager_->log_timeslice_transmit_time(compute_index, timeslice,
                                                  size);
}

bool InputSchedulerOrchestrator::mark_timeslice_rdma_write_acked(
    uint32_t compute_index, uint64_t timeslice) {
  return timeslice_manager_->acknowledge_timeslice_rdma_write(compute_index,
                                                              timeslice);
}

void InputSchedulerOrchestrator::mark_timeslices_acked(
    uint32_t compute_index, uint64_t up_to_descriptor_id) {
  uint64_t last_descriptor =
      timeslice_manager_->get_last_acked_descriptor(compute_index);
  for (uint64_t desc = last_descriptor + 1; desc <= up_to_descriptor_id;
       ++desc) {
    uint64_t timeslice = get_timeslice_by_descriptor(compute_index, desc);
    if (timeslice == ConstVariables::MINUS_ONE) {
      L_(warning) << "Desc " << desc << " in compute conn_" << compute_index
                  << " does not exist in the TimesliceManager database!!!";
      continue;
    }
    interval_scheduler_->increament_acked_timeslices(timeslice);
  }

  /*uint64_t avg_latency = */ timeslice_manager_
      ->acknowledge_timeslices_completion(compute_index, up_to_descriptor_id);
  // TODO if (avg_latency > 0)
  // heartbeat_manager_->log_new_latency(compute_index, avg_latency);
}

bool InputSchedulerOrchestrator::is_timeslice_rdma_acked(uint32_t compute_index,
                                                         uint64_t timeslice) {
  return timeslice_manager_->is_timeslice_rdma_acked(compute_index, timeslice);
}

uint64_t
InputSchedulerOrchestrator::get_last_acked_descriptor(uint32_t compute_index) {
  return timeslice_manager_->get_last_acked_descriptor(compute_index);
}

uint64_t
InputSchedulerOrchestrator::get_timeslice_by_descriptor(uint32_t compute_index,
                                                        uint64_t descriptor) {
  return timeslice_manager_->get_timeslice_by_descriptor(compute_index,
                                                         descriptor);
}

uint64_t InputSchedulerOrchestrator::get_last_connection_descriptor_index(
    uint32_t compute_index) {
  return timeslice_manager_->get_last_connection_descriptor_index(
      compute_index);
}

std::pair<uint64_t, uint64_t>
InputSchedulerOrchestrator::get_data_and_desc_of_timeslice(
    uint32_t compute_index, uint32_t timeslice) {
  return timeslice_manager_->get_data_and_desc_of_timeslice(compute_index,
                                                            timeslice);
}

std::pair<uint64_t, uint64_t>
InputSchedulerOrchestrator::get_data_and_desc_of_last_timeslice(
    uint32_t compute_index) {
  return timeslice_manager_->get_data_and_desc_of_last_timeslice(compute_index);
}

std::pair<uint64_t, uint64_t>
InputSchedulerOrchestrator::get_data_and_desc_of_last_rdma_acked_timeslice(
    uint32_t compute_index) {
  uint64_t timeslice =
      timeslice_manager_->get_last_rdma_acked_timeslice(compute_index);
  return get_data_and_desc_of_timeslice(compute_index, timeslice);
}

void InputSchedulerOrchestrator::consider_reschedule_decision(
    HeartbeatFailedNodeInfo failed_node_info) {
  if (is_decision_considered(failed_node_info.index))
    return;

  mark_timeslices_acked(failed_node_info.index,
                        failed_node_info.last_completed_desc);
  L_(debug) << "consider_reschedule_decision of " << failed_node_info.index
            << " sent " << get_sent_timeslices();
  std::vector<uint64_t> undo_timeslices =
      timeslice_manager_->consider_reschedule_decision(
          failed_node_info, retrieve_timeout_connections());
  interval_scheduler_->undo_increament_sent_timeslices(undo_timeslices);
  // TODO
  // interval_scheduler_->update_compute_connection_count(interval_scheduler_->get_compute_connection_count()
  // - 1);
  L_(debug) << "Undo " << undo_timeslices.size()
            << " .... new sent_timeslices = " << get_sent_timeslices();

  assert(heartbeat_manager_->remove_timeslice_blockage_trigger(
      failed_node_info.index));

  // TODO
  // InputSchedulerOrchestrator::SHOW_LOGS_ = true;
}

bool InputSchedulerOrchestrator::is_decision_considered(
    uint32_t connection_id) {
  return timeslice_manager_->is_decision_considered(connection_id);
}

void InputSchedulerOrchestrator::update_data_source_desc(
    uint64_t data_source_desc) {
  timeslice_manager_->update_data_source_desc(data_source_desc);
}

uint64_t InputSchedulerOrchestrator::get_sent_timeslices() {
  return timeslice_manager_->get_sent_timeslices();
}

void InputSchedulerOrchestrator::log_timeslice_IB_blocked(
    uint32_t compute_index, uint64_t timeslice, bool sent_completed) {
  uint64_t duration =
      timeslice_manager_->log_timeslice_IB_blocked(timeslice, sent_completed);
  if (sent_completed && duration > ConstVariables::ZERO) {
    interval_scheduler_->add_input_buffer_blockage_duration(
        compute_index, timeslice, duration);
  }
}

void InputSchedulerOrchestrator::log_timeslice_CB_blocked(
    uint32_t compute_index, uint64_t timeslice, bool sent_completed) {
  uint64_t duration =
      timeslice_manager_->log_timeslice_CB_blocked(timeslice, sent_completed);
  if (sent_completed && duration > ConstVariables::ZERO) {
    interval_scheduler_->add_compute_buffer_blockage_duration(
        compute_index, timeslice, duration);
  }
}

void InputSchedulerOrchestrator::log_timeslice_MR_blocked(
    uint32_t /*compute_index*/, uint64_t timeslice, bool sent_completed) {
  timeslice_manager_->log_timeslice_MR_blocked(timeslice, sent_completed);
}

//// Methods combine data from different objects

HeartbeatFailedNodeInfo*
InputSchedulerOrchestrator::get_timed_out_connection(int32_t timeout_conn) {
  if (timeout_conn != -1 && is_decision_considered(timeout_conn))
    return mark_connection_timed_out(timeout_conn);
  if (timeout_conn == -1)
    timeout_conn = get_new_timeout_connection();

  if (timeout_conn != -1) {
    InputIntervalInfo current_interval =
        interval_scheduler_->get_current_interval_info();
    HeartbeatFailedNodeInfo* failure_info = mark_connection_timed_out(
        timeout_conn, get_last_acked_descriptor(timeout_conn),
        timeslice_manager_->get_last_timeslice_before_blockage(
            timeout_conn, retrieve_timeout_connections(),
            current_interval.start_ts, current_interval.end_ts));
    return failure_info;
  }
  return new HeartbeatFailedNodeInfo();
}

//// Variables

InputIntervalScheduler* InputSchedulerOrchestrator::interval_scheduler_ =
    nullptr;
InputTimesliceManager* InputSchedulerOrchestrator::timeslice_manager_ = nullptr;
InputHeartbeatManager* InputSchedulerOrchestrator::heartbeat_manager_ = nullptr;

bool InputSchedulerOrchestrator::SHOW_LOGS_ = false;

} // namespace tl_libfabric
