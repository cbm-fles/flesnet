/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */
#pragma once

#include "Monitor.hpp"
#include "Scheduler.hpp"
#include "SubTimeslice.hpp"
#include <csignal>
#include <cstdint>
#include <deque>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>
#include <unistd.h>

// TsScheduler: Receive subtimeslice announcements from stsenders, aggregate,
// and send subtimeslice handles to tsbuilders

struct SenderConnection {
  SenderInfo info;
  ucp_ep_h ep = nullptr;
  struct StDesc {
    TsId id;
    uint64_t ms_data_size = 0;
    StDescriptor st_descriptor;
  };
  std::deque<StDesc> announced_st;
  TsId last_received_st = 0;
};

struct BuilderConnection {
  BuilderInfo info;
  ucp_ep_h ep = nullptr;
  uint64_t bytes_available = 0;
  bool is_out_of_memory = false;
};

struct StatusInfo {
  size_t timeslice_count = 0; ///< total number of processed timeslices
  size_t component_count = 0; ///< total number of processed components
  size_t data_bytes = 0;      ///< total number of processed data bytes
  size_t timeslice_discarded_count = 0; ///< number of discarded timeslices

  StatusInfo operator-(const StatusInfo& other) const {
    StatusInfo result;
    result.timeslice_count = timeslice_count - other.timeslice_count;
    result.component_count = component_count - other.component_count;
    result.data_bytes = data_bytes - other.data_bytes;
    result.timeslice_discarded_count =
        timeslice_discarded_count - other.timeslice_discarded_count;
    return result;
  }

  bool operator==(const StatusInfo& other) const = default;
};

class TsScheduler {
public:
  TsScheduler(volatile sig_atomic_t* signal_status,
              uint16_t listen_port,
              int64_t timeslice_duration_ns,
              int64_t timeout_ns,
              cbm::Monitor* monitor);
  ~TsScheduler();
  TsScheduler(const TsScheduler&) = delete;
  TsScheduler& operator=(const TsScheduler&) = delete;

  void run();

private:
  volatile sig_atomic_t* m_signal_status;
  Scheduler m_tasks;

  uint16_t m_listen_port;
  int64_t m_timeslice_duration_ns;
  int64_t m_timeout_ns;
  std::string m_hostname;
  cbm::Monitor* m_monitor = nullptr;

  int m_epoll_fd = -1;
  std::unordered_map<ucs_status_ptr_t, TsId> m_active_send_requests;

  ucp_context_h m_context = nullptr;
  ucp_worker_h m_worker = nullptr;
  ucp_listener_h m_listener = nullptr;
  std::unordered_map<ucp_ep_h, std::string> m_connections;
  std::unordered_map<ucp_ep_h, SenderConnection> m_senders;
  std::vector<BuilderConnection> m_builders;
  std::size_t m_ts_count = 0;
  uint64_t m_id = 0;

  StatusInfo m_status_info = {};
  StatusInfo m_status_info_last = {};
  std::chrono::system_clock::time_point m_status_time_last;

  // Connection management
  void handle_new_connection(ucp_conn_request_h conn_request);
  void handle_endpoint_error(ucp_ep_h ep, ucs_status_t status);
  void disconnect_from_all();

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
  void send_release_to_senders(TsId id);

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
  void assign_timeslice(TsId id);
  void send_assignment_to_builder(const StCollection& coll,
                                  BuilderConnection& builder);

  // Helper methods
  StCollection create_collection_descriptor(TsId id);
  void report_status();
  void log_status();

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
