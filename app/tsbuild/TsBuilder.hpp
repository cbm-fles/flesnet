// Copyright 2025 Jan de Cuveland
#pragma once

#include "Monitor.hpp"
#include "Scheduler.hpp"
#include "SubTimeslice.hpp"
#include "System.hpp"
#include "TsBuffer.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string_view>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <thread>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>
#include <unistd.h>
#include <unordered_map>

// TsBuilder: Receive timeslice announcements from tssched, connect to senders,
// receive subtimeslices and aggregate the data to timeslices

enum class StState : uint8_t {
  Allocated = 0,
  Requested = 1,
  Receiving = 2,
  Complete = 3,
  Failed = 4
};

struct TsHandle {
  TsHandle(std::byte* buffer, StCollection contributions)
      : id(contributions.id), allocated_at_ns(fles::system::current_time_ns()),
        buffer(buffer), sender_ids(std::move(contributions.sender_ids)),
        ms_data_sizes(std::move(contributions.ms_data_sizes)),
        offsets(sender_ids.size()), iovectors(sender_ids.size()),
        serialized_descriptors(sender_ids.size()),
        descriptors(sender_ids.size()), states(sender_ids.size()) {
    // Initialize offsets
    std::partial_sum(ms_data_sizes.begin(), ms_data_sizes.end() - 1,
                     offsets.begin() + 1);
    // Initialize states
    std::fill(states.begin(), states.end(), StState::Allocated);
  }

  // Cannot be moved or copied (pointer to data is used by ucx)
  TsHandle(const TsHandle&) = delete;
  TsHandle& operator=(const TsHandle&) = delete;

  const TsId id;
  const uint64_t allocated_at_ns;
  uint64_t published_at_ns = 0;
  std::byte* const buffer;
  std::vector<std::string> sender_ids; // IDs of the senders
  std::vector<uint64_t> ms_data_sizes; // Sizes of the content data
  std::vector<uint64_t> offsets;
  std::vector<std::array<ucp_dt_iov, 2>> iovectors;
  std::vector<std::vector<std::byte>> serialized_descriptors;
  std::vector<StDescriptor> descriptors;
  std::vector<StState> states;
  bool is_published = false;
};

class TsBuilder {
public:
  TsBuilder(TsBuffer& timeslice_buffer,
            std::string_view scheduler_address,
            int64_t timeout_ns,
            cbm::Monitor* monitor);
  ~TsBuilder();
  TsBuilder(const TsBuilder&) = delete;
  TsBuilder& operator=(const TsBuilder&) = delete;

private:
  Scheduler tasks_;
  TsBuffer& timeslice_buffer_;

  std::string scheduler_address_;
  int64_t timeout_ns_;
  std::string hostname_;
  cbm::Monitor* monitor_ = nullptr;

  int epoll_fd_ = -1;
  ucp_context_h context_ = nullptr;
  ucp_worker_h worker_ = nullptr;

  std::unordered_map<std::string, ucp_ep_h> sender_to_ep_;
  std::unordered_map<ucp_ep_h, std::string> ep_to_sender_;

  std::unordered_map<TsId, std::unique_ptr<TsHandle>> ts_handles_;
  std::unordered_map<ucs_status_ptr_t, std::pair<TsId, std::size_t>>
      active_data_recv_requests_;

  ucp_ep_h scheduler_ep_ = nullptr;
  bool scheduler_connecting_ = false;
  bool scheduler_connected_ = false;

  size_t timeslice_count_ = 0; ///< total number of assigned timeslices
  size_t component_count_ = 0; ///< total number of received components
  size_t byte_count_ = 0;      ///< total number of processed bytes
  size_t timeslice_incomplete_count_ = 0; ///< number of incomplete timeslices

  std::jthread worker_thread_;

  // Main operation loop
  void operator()(std::stop_token stop_token);

  // Scheduler connection management
  void connect_to_scheduler_if_needed();
  void connect_to_scheduler();
  void handle_scheduler_error(ucp_ep_h ep, ucs_status_t status);
  bool register_with_scheduler();
  void handle_scheduler_register_complete(ucs_status_ptr_t request,
                                          ucs_status_t status);
  void disconnect_from_scheduler(bool force = false);

  // Scheduler message handling
  void send_status_to_scheduler(uint64_t event, TsId id);
  void send_periodic_status_to_scheduler();
  ucs_status_t handle_scheduler_send_ts(const void* header,
                                        size_t header_length,
                                        void* data,
                                        size_t length,
                                        const ucp_am_recv_param_t* param);

  // Sender connection management
  void connect_to_sender(const std::string& sender_id);
  void handle_sender_error(ucp_ep_h ep, ucs_status_t status);
  void disconnect_from_senders();

  // Sender message handling
  void send_request_to_sender(const std::string& sender_id, TsId id);
  ucs_status_t handle_sender_data(const void* header,
                                  size_t header_length,
                                  void* data,
                                  size_t length,
                                  const ucp_am_recv_param_t* param);
  void handle_sender_data_recv_complete(void* request,
                                        ucs_status_t status,
                                        size_t length);

  // Queue processing
  void process_completion(TsId id);

  // Helper methods
  void update_st_state(TsHandle& tsh,
                       std::size_t contribution_index,
                       StState new_state);
  StDescriptor create_timeslice_descriptor(TsHandle& tsh);
  void report_status();

  // UCX static callbacks (trampolines)
  static void on_scheduler_error(void* arg, ucp_ep_h ep, ucs_status_t status) {
    static_cast<TsBuilder*>(arg)->handle_scheduler_error(ep, status);
  }
  static void on_scheduler_register_complete(void* request,
                                             ucs_status_t status,
                                             void* user_data) {
    static_cast<TsBuilder*>(user_data)->handle_scheduler_register_complete(
        request, status);
  }
  static ucs_status_t on_scheduler_send_ts(void* arg,
                                           const void* header,
                                           size_t header_length,
                                           void* data,
                                           size_t length,
                                           const ucp_am_recv_param_t* param) {
    return static_cast<TsBuilder*>(arg)->handle_scheduler_send_ts(
        header, header_length, data, length, param);
  }
  static void on_sender_error(void* arg, ucp_ep_h ep, ucs_status_t status) {
    static_cast<TsBuilder*>(arg)->handle_sender_error(ep, status);
  }
  static ucs_status_t on_sender_data(void* arg,
                                     const void* header,
                                     size_t header_length,
                                     void* data,
                                     size_t length,
                                     const ucp_am_recv_param_t* param) {
    return static_cast<TsBuilder*>(arg)->handle_sender_data(
        header, header_length, data, length, param);
  }
  static void on_sender_data_recv_complete(void* request,
                                           ucs_status_t status,
                                           size_t length,
                                           void* user_data) {
    static_cast<TsBuilder*>(user_data)->handle_sender_data_recv_complete(
        request, status, length);
  }
};
