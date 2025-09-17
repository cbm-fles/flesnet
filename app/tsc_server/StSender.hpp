// Copyright 2025 Jan de Cuveland
#pragma once

#include "Scheduler.hpp"
#include "SubTimeslice.hpp"
#include <cstddef>
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

using namespace std::chrono_literals;

// StSender: Announce subtimeslices to tssched and send them to tsbuilders

struct AnnouncementHandle {
  AnnouncementHandle(TsId id,
                     std::vector<std::byte> st_descriptor_bytes,
                     std::vector<ucp_dt_iov> iov_vector)
      : id(id), st_descriptor_bytes(std::move(st_descriptor_bytes)),
        iov_vector(std::move(iov_vector)) {}

  // Cannot be moved or copied (pointer to data is used by ucx)
  AnnouncementHandle(const AnnouncementHandle&) = delete;
  AnnouncementHandle& operator=(const AnnouncementHandle&) = delete;

  const TsId id;
  std::vector<std::byte> st_descriptor_bytes;
  std::vector<ucp_dt_iov> iov_vector;
  size_t active_send_requests = 0;
  bool pending_release = false;
};

class StSender {
public:
  StSender(std::string_view scheduler_address, uint16_t listen_port);
  ~StSender();
  StSender(const StSender&) = delete;
  StSender& operator=(const StSender&) = delete;

  // Public API methods
  void announce_subtimeslice(TsId id, const StHandle& sth);
  void retract_subtimeslice(TsId id);
  std::optional<TsId> try_receive_completion();

private:
  Scheduler m_tasks;

  std::string m_scheduler_address;
  uint16_t m_listen_port;
  std::string m_sender_id;

  int m_queue_event_fd = -1;
  std::deque<std::pair<TsId, StHandle>> m_pending_announcements;
  std::deque<TsId> m_pending_retractions;
  std::mutex m_queue_mutex;
  std::queue<TsId> m_completed;
  std::mutex m_completions_mutex;
  int m_epoll_fd = -1;

  std::unordered_map<TsId, std::unique_ptr<AnnouncementHandle>> m_announced;
  std::unordered_map<ucs_status_ptr_t, TsId> m_active_send_requests;

  ucp_context_h m_context = nullptr;
  ucp_worker_h m_worker = nullptr;
  ucp_listener_h m_listener = nullptr;
  std::unordered_map<ucp_ep_h, std::string> m_builders;

  static constexpr auto m_scheduler_retry_interval = 2s;
  bool m_mute_scheduler_reconnect = false;

  ucp_ep_h m_scheduler_ep = nullptr;
  bool m_scheduler_connecting = false;
  bool m_scheduler_connected = false;

  std::jthread m_worker_thread;

  // Main operation loop
  void operator()(std::stop_token stop_token);

  // Scheduler connection management
  void connect_to_scheduler_if_needed();
  void connect_to_scheduler();
  void handle_scheduler_error(ucp_ep_h ep, ucs_status_t status);
  void handle_scheduler_register_complete(ucs_status_ptr_t request,
                                          ucs_status_t status);
  void disconnect_from_scheduler(bool force = false);

  // Scheduler message handling
  void do_announce_subtimeslice(TsId id, const StHandle& sth);
  void do_retract_subtimeslice(TsId id);
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
  void send_subtimeslice_to_builder(TsId id, ucp_ep_h ep);
  void handle_builder_send_complete(void* request, ucs_status_t status);
  void disconnect_from_builders();

  // Queue processing
  void notify_queue_update() const;
  std::size_t process_queues();
  void flush_announced();

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
