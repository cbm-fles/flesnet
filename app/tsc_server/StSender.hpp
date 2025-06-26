// Copyright 2025 Jan de Cuveland
#pragma once

#include "Scheduler.hpp"
#include "SubTimeslice.hpp"
#include <array>
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

// Announce subtimeslices to tssched and send them to tsbuilders
class StSender {
public:
  using StID = uint64_t;

  StSender(uint16_t listen_port, std::string_view scheduler_address);
  ~StSender();
  StSender(const StSender&) = delete;
  StSender& operator=(const StSender&) = delete;

  // Public API methods
  void announce_subtimeslice(StID id, const SubTimesliceHandle& st);
  void retract_subtimeslice(StID id);
  std::optional<StID> try_receive_completion();

private:
  // stsender (connect) -> tssched (listen)
  static constexpr unsigned int AM_SENDER_REGISTER = 20;
  static constexpr unsigned int AM_SENDER_ANNOUNCE_ST = 21;
  static constexpr unsigned int AM_SENDER_RETRACT_ST = 22;
  // stsender (connect) <- tssched (listen)
  static constexpr unsigned int AM_SCHED_RELEASE_ST = 30;
  // stsender (listen) <- tsbuilder (connect)
  static constexpr unsigned int AM_BUILDER_REQUEST_ST = 40;
  // stsender (listen) -> tsbuilder (connect)
  static constexpr unsigned int AM_SENDER_SEND_ST = 50;

  Scheduler tasks_;

  uint16_t listen_port_;
  std::string scheduler_address_;
  uint16_t scheduler_port_ = 13373;
  std::string sender_id_;

  int queue_event_fd_ = -1;
  std::deque<std::pair<StID, SubTimesliceHandle>> pending_announcements_;
  std::deque<std::size_t> pending_retractions_;
  std::mutex queue_mutex_;
  std::queue<std::size_t> completed_sts_;
  std::mutex completions_mutex_;
  int ucx_event_fd_ = -1;
  int epoll_fd_ = -1;

  std::unordered_map<StID, std::pair<std::string, std::vector<ucp_dt_iov>>>
      announced_sts_;
  std::unordered_map<ucs_status_ptr_t, StID> active_send_requests_;

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

  // Network initialization/cleanup
  bool initialize_ucx();
  bool create_listener();
  void cleanup();

  // Scheduler connection management
  void connect_to_scheduler_if_needed();
  void connect_to_scheduler();
  void handle_scheduler_error(ucp_ep_h ep, ucs_status_t status);
  bool register_with_scheduler();
  void handle_scheduler_register_complete(ucs_status_ptr_t request,
                                          ucs_status_t status);
  void disconnect_from_scheduler(bool force = false);

  // Scheduler message handling
  void send_announcement_to_scheduler(StID id);
  void send_retraction_to_scheduler(StID id);
  void handle_scheduler_send_complete(void* request, ucs_status_t status);
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
  void send_subtimeslice_to_builder(StID id, ucp_ep_h ep);
  void handle_builder_send_complete(void* request, ucs_status_t status);

  // Queue processing
  void notify_queue_update() const;
  std::size_t process_queues();
  void process_announcement(StID id, const SubTimesliceHandle& st_d);
  void process_retraction(StID id);
  void complete_subtimeslice(std::size_t id);
  void flush_announced();

  // Helper methods
  StDescriptor create_subtimeslice_handle(const SubTimesliceHandle& st_d);
  std::vector<ucp_dt_iov> create_iov_vector(const SubTimesliceHandle& st,
                                            const std::string& serialized);

  // UCX event handling
  bool arm_worker_and_wait(std::array<epoll_event, 1>& events);

  // UCX static callbacks (trampolines)
  static void on_new_connection(ucp_conn_request_h conn_request, void* arg);
  static void on_endpoint_error(void* arg, ucp_ep_h ep, ucs_status_t status);
  static void on_scheduler_error(void* arg, ucp_ep_h ep, ucs_status_t status);
  static ucs_status_t on_builder_request(void* arg,
                                         const void* header,
                                         size_t header_length,
                                         void* data,
                                         size_t length,
                                         const ucp_am_recv_param_t* param);
  static ucs_status_t on_scheduler_release(void* arg,
                                           const void* header,
                                           size_t header_length,
                                           void* data,
                                           size_t length,
                                           const ucp_am_recv_param_t* param);
  static void
  on_builder_send_complete(void* request, ucs_status_t status, void* user_data);
  static void on_scheduler_register_complete(void* request,
                                             ucs_status_t status,
                                             void* user_data);
  static void on_scheduler_send_complete(void* request,
                                         ucs_status_t status,
                                         void* user_data);
};
