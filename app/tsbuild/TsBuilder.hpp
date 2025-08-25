// Copyright 2025 Jan de Cuveland
#pragma once

#include "Scheduler.hpp"
#include "SubTimeslice.hpp"
#include "System.hpp"
#include "TimesliceBufferFlex.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
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

struct SenderConnection {
  std::string sender_id;
  ucp_ep_h ep;
};

enum class StState : uint8_t {
  Allocated = 0,
  Requested = 1,
  Receiving = 2,
  Complete = 3,
  Failed = 4
};

struct TsHandler {
  TsHandler(std::byte* buffer, StCollectionDescriptor desc)
      : id(desc.id), allocated_at_ns_(fles::system::current_time_ns()),
        buffer(buffer), contributions(std::move(desc.contributions)),
        offsets(contributions.size()), iovectors(contributions.size()),
        descriptors(contributions.size()), states(contributions.size()) {
    // Compute offsets from TsContribution.content_size
    offsets.resize(contributions.size());
    std::size_t offset = 0;
    for (std::size_t i = 0; i < contributions.size(); ++i) {
      offsets[i] = offset;
      offset += contributions[i].content_size;
    }
    // Initialize serialized_descriptors and iovectors
    iovectors.resize(contributions.size());
    serialized_descriptors.resize(contributions.size());
    for (std::size_t i = 0; i < contributions.size(); ++i) {
      serialized_descriptors[i].resize(contributions[i].desc_size);
      iovectors[i][0] = {serialized_descriptors[i].data(),
                         contributions[i].desc_size};
      iovectors[i][1] = {buffer + offsets[i], contributions[i].content_size};
    }
  }

  // Cannot be moved or copied (pointer to data is used by ucx)
  TsHandler(const TsHandler&) = delete;
  TsHandler& operator=(const TsHandler&) = delete;

  const TsID id;
  const uint64_t allocated_at_ns_;
  std::byte* const buffer;
  std::vector<TsContribution> contributions;
  std::vector<uint64_t> offsets;
  std::vector<std::array<ucp_dt_iov, 2>> iovectors;
  std::vector<std::vector<std::byte>> serialized_descriptors;
  std::vector<StDescriptor> descriptors;
  std::vector<StState> states;
  bool is_published = false;
};

class TsBuilder {
public:
  TsBuilder(TimesliceBufferFlex& timeslice_buffer,
            std::string_view scheduler_address,
            int64_t timeout_ns);
  ~TsBuilder();
  TsBuilder(const TsBuilder&) = delete;
  TsBuilder& operator=(const TsBuilder&) = delete;

private:
  Scheduler tasks_;
  TimesliceBufferFlex& timeslice_buffer_;

  std::string scheduler_address_;
  int64_t timeout_ns_;
  std::string builder_id_;

  int epoll_fd_ = -1;
  ucp_context_h context_ = nullptr;
  ucp_worker_h worker_ = nullptr;
  std::unordered_map<ucp_ep_h, std::string> connections_;
  std::vector<SenderConnection> senders_;
  std::vector<std::unique_ptr<TsHandler>> ts_handlers_;
  std::unordered_map<ucs_status_ptr_t, std::pair<TsID, std::size_t>>
      active_data_recv_requests_;

  ucp_ep_h scheduler_ep_ = nullptr;
  bool scheduler_connecting_ = false;
  bool scheduler_connected_ = false;

  uint64_t bytes_available_ = 0; // TODO: use!
  uint64_t bytes_processed_ = 0; // TODO: use!
  // uint64_t bytes_assigned = 0;

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
  void send_status_to_scheduler(uint64_t event, TsID id);
  ucs_status_t handle_scheduler_send_ts(const void* header,
                                        size_t header_length,
                                        void* data,
                                        size_t length,
                                        const ucp_am_recv_param_t* param);

  // Sender connection management
  void connect_to_sender(std::string sender_id);
  void handle_sender_error(ucp_ep_h ep, ucs_status_t status);
  void disconnect_from_senders();

  // Sender message handling
  void send_request_to_sender(std::string_view sender_id, TsID id);
  ucs_status_t handle_sender_data(const void* header,
                                  size_t header_length,
                                  void* data,
                                  size_t length,
                                  const ucp_am_recv_param_t* param);
  void handle_sender_data_recv_complete(void* request,
                                        ucs_status_t status,
                                        size_t length);

  // Queue processing
  void process_completion(TsID id);

  // Helper methods
  void update_st_state(TsHandler& tsh,
                       std::size_t contribution_index,
                       StState new_state);
  StDescriptor create_timeslice_descriptor(TsHandler& tsh);

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
