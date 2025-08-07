// Copyright 2025 Jan de Cuveland
#pragma once

#include "SubTimeslice.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <thread>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>
#include <unistd.h>

// TsScheduler: Receive subtimeslice announcements from stsenders, aggregate,
// and send subtimeslice handles to tsbuilders

struct SenderConnection {
  std::string id;
  ucp_ep_h ep;
  struct StDesc {
    StID id;
    uint64_t desc_size;
    uint64_t content_size;
  };
  std::deque<StDesc> announced_st;
};

struct BuilderConnection {
  std::string id;
  ucp_ep_h ep;
  uint64_t bytes_available = 0;
  uint64_t bytes_processed = 0;
  uint64_t bytes_assigned = 0;
};

class TsScheduler {
public:
  TsScheduler(uint16_t listen_port);
  ~TsScheduler();
  TsScheduler(const TsScheduler&) = delete;
  TsScheduler& operator=(const TsScheduler&) = delete;

private:
  uint16_t listen_port_;
  int epoll_fd_ = -1;
  std::unordered_map<ucs_status_ptr_t, StID> active_send_requests_;

  ucp_context_h context_ = nullptr;
  ucp_worker_h worker_ = nullptr;
  ucp_listener_h listener_ = nullptr;
  std::unordered_map<ucp_ep_h, std::string> connections_;
  std::unordered_map<ucp_ep_h, SenderConnection> sender_connections_;
  std::unordered_map<ucp_ep_h, BuilderConnection> builder_connections_;

  std::jthread worker_thread_;

  // Main operation loop
  void operator()(std::stop_token stop_token);

  // Connection management
  void handle_new_connection(ucp_conn_request_h conn_request);
  void handle_endpoint_error(ucp_ep_h ep, ucs_status_t status);

  // Sender message handling
  ucs_status_t handle_sender_register(const void* header,
                                      size_t header_length,
                                      void* data,
                                      size_t length,
                                      const ucp_am_recv_param_t* param);
  ucs_status_t handle_sender_announce(const void* header,
                                      size_t header_length,
                                      void* data,
                                      size_t length,
                                      const ucp_am_recv_param_t* param);
  ucs_status_t handle_sender_retract(const void* header,
                                     size_t header_length,
                                     void* data,
                                     size_t length,
                                     const ucp_am_recv_param_t* param);
  void send_release_to_senders(StID id);

  // Builder message handling
  ucs_status_t handle_builder_register(const void* header,
                                       size_t header_length,
                                       void* data,
                                       size_t length,
                                       const ucp_am_recv_param_t* param);
  ucs_status_t handle_builder_status(const void* header,
                                     size_t header_length,
                                     void* data,
                                     size_t length,
                                     const ucp_am_recv_param_t* param);
  void send_timeslice_to_builder(StID id, ucp_ep_h ep);

  // UCX event handling
  bool arm_worker_and_wait(std::array<epoll_event, 1>& events);

  // UCX static callbacks (trampolines)
  static void on_new_connection(ucp_conn_request_h conn_request, void* arg) {
    static_cast<TsScheduler*>(arg)->handle_new_connection(conn_request);
  }
  static void on_endpoint_error(void* arg, ucp_ep_h ep, ucs_status_t status) {
    static_cast<TsScheduler*>(arg)->handle_endpoint_error(ep, status);
  }
  static ucs_status_t on_sender_register(void* arg,
                                         const void* header,
                                         size_t header_length,
                                         void* data,
                                         size_t length,
                                         const ucp_am_recv_param_t* param) {
    return static_cast<TsScheduler*>(arg)->handle_sender_register(
        header, header_length, data, length, param);
  }
  static ucs_status_t on_sender_announce(void* arg,
                                         const void* header,
                                         size_t header_length,
                                         void* data,
                                         size_t length,
                                         const ucp_am_recv_param_t* param) {
    return static_cast<TsScheduler*>(arg)->handle_sender_announce(
        header, header_length, data, length, param);
  }
  static ucs_status_t on_sender_retract(void* arg,
                                        const void* header,
                                        size_t header_length,
                                        void* data,
                                        size_t length,
                                        const ucp_am_recv_param_t* param) {
    return static_cast<TsScheduler*>(arg)->handle_sender_retract(
        header, header_length, data, length, param);
  }
  static ucs_status_t on_builder_register(void* arg,
                                          const void* header,
                                          size_t header_length,
                                          void* data,
                                          size_t length,
                                          const ucp_am_recv_param_t* param) {
    return static_cast<TsScheduler*>(arg)->handle_builder_register(
        header, header_length, data, length, param);
  }
  static ucs_status_t on_builder_status(void* arg,
                                        const void* header,
                                        size_t header_length,
                                        void* data,
                                        size_t length,
                                        const ucp_am_recv_param_t* param) {
    return static_cast<TsScheduler*>(arg)->handle_builder_status(
        header, header_length, data, length, param);
  }
};
