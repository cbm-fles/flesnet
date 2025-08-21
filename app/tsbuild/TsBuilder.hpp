// Copyright 2025 Jan de Cuveland
#pragma once

#include "Scheduler.hpp"
#include "SubTimeslice.hpp"
#include "TimesliceBufferFlex.hpp"
#include <cstdint>
#include <string_view>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <thread>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>
#include <unistd.h>

// TsBuilder: Receive timeslice announcements from tssched, connect to senders,
// receive subtimeslices and aggregate the data to timeslices

struct SenderConnection {
  std::string id;
  ucp_ep_h ep;
  // struct StDesc {
  //   StID id;
  //   uint64_t desc_size;
  //   uint64_t content_size;
  // };
  // std::deque<StDesc> announced_st;
  // StID last_received_st = 0;
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
  void send_status_to_scheduler();
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
  void send_request_to_sender(StID id);
  ucs_status_t handle_sender_data(const void* header,
                                  size_t header_length,
                                  void* data,
                                  size_t length,
                                  const ucp_am_recv_param_t* param);

  // Queue processing
  std::size_t process_queues();

  // Helper methods
  TsDescriptor create_timeslice_descriptor(StID id);

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
};
