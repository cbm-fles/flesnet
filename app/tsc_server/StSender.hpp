// Copyright 2025 Jan de Cuveland
#pragma once

#include "Scheduler.hpp"
#include "SubTimeslice.hpp"
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <thread>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

// StSender: Announce subtimeslices to tssched and send them to tsbuilders

class StSender {
public:
  StSender(std::string_view scheduler_address, uint16_t listen_port);
  ~StSender();
  StSender(const StSender&) = delete;
  StSender& operator=(const StSender&) = delete;

  // Public API methods
  void announce_subtimeslice(TsID id, const SubTimesliceHandle& sth);
  void retract_subtimeslice(TsID id);
  std::optional<TsID> try_receive_completion();

private:
  Scheduler tasks_;

  std::string scheduler_address_;
  uint16_t listen_port_;
  std::string sender_id_;

  int queue_event_fd_ = -1;
  std::deque<std::pair<TsID, SubTimesliceHandle>> pending_announcements_;
  std::deque<TsID> pending_retractions_;
  std::mutex queue_mutex_;
  std::queue<TsID> completed_;
  std::mutex completions_mutex_;
  int epoll_fd_ = -1;

  std::unordered_map<TsID,
                     std::pair<std::vector<std::byte>, std::vector<ucp_dt_iov>>>
      announced_;
  std::unordered_map<ucs_status_ptr_t, TsID> active_send_requests_;

  ucp_context_h context_ = nullptr;
  ucp_worker_h worker_ = nullptr;
  ucp_listener_h listener_ = nullptr;
  std::unordered_map<ucp_ep_h, std::string> connections_;

  ucp_ep_h scheduler_ep_ = nullptr;
  bool scheduler_connecting_ = false;
  bool scheduler_connected_ = false;

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
  void send_announcement_to_scheduler(TsID id);
  void send_retraction_to_scheduler(TsID id);
  ucs_status_t handle_scheduler_release(const void* header,
                                        size_t header_length,
                                        void* data,
                                        size_t length,
                                        const ucp_am_recv_param_t* param);

  // Builder connection management
  void handle_new_connection(ucp_conn_request_h conn_request);
  void handle_endpoint_error(ucp_ep_h ep, ucs_status_t status);

  // Builder message handling
  ucs_status_t handle_builder_request(const void* header,
                                      size_t header_length,
                                      void* data,
                                      size_t length,
                                      const ucp_am_recv_param_t* param);
  void send_subtimeslice_to_builder(TsID id, ucp_ep_h ep);
  void handle_builder_send_complete(void* request, ucs_status_t status);

  // Queue processing
  void notify_queue_update() const;
  std::size_t process_queues();
  void process_announcement(TsID id, const SubTimesliceHandle& sth);
  void process_retraction(TsID id);
  void complete_subtimeslice(TsID id);
  void flush_announced();

  // Helper methods
  static StDescriptor
  create_subtimeslice_descriptor(const SubTimesliceHandle& sth);
  static std::vector<ucp_dt_iov>
  create_iov_vector(const SubTimesliceHandle& sth,
                    std::span<std::byte> descriptor_bytes);

  // UCX static callbacks (trampolines)
  static void on_new_connection(ucp_conn_request_h conn_request, void* arg) {
    static_cast<StSender*>(arg)->handle_new_connection(conn_request);
  }
  static void on_endpoint_error(void* arg, ucp_ep_h ep, ucs_status_t status) {
    static_cast<StSender*>(arg)->handle_endpoint_error(ep, status);
  }
  static void on_scheduler_error(void* arg, ucp_ep_h ep, ucs_status_t status) {
    static_cast<StSender*>(arg)->handle_scheduler_error(ep, status);
  }
  static ucs_status_t on_builder_request(void* arg,
                                         const void* header,
                                         size_t header_length,
                                         void* data,
                                         size_t length,
                                         const ucp_am_recv_param_t* param) {
    return static_cast<StSender*>(arg)->handle_builder_request(
        header, header_length, data, length, param);
  }
  static ucs_status_t on_scheduler_release(void* arg,
                                           const void* header,
                                           size_t header_length,
                                           void* data,
                                           size_t length,
                                           const ucp_am_recv_param_t* param) {
    return static_cast<StSender*>(arg)->handle_scheduler_release(
        header, header_length, data, length, param);
  }
  static void on_builder_send_complete(void* request,
                                       ucs_status_t status,
                                       void* user_data) {
    static_cast<StSender*>(user_data)->handle_builder_send_complete(request,
                                                                    status);
  }
  static void on_scheduler_register_complete(void* request,
                                             ucs_status_t status,
                                             void* user_data) {
    static_cast<StSender*>(user_data)->handle_scheduler_register_complete(
        request, status);
  }
};
