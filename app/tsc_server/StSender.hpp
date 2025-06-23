// Copyright 2025 Jan de Cuveland
#pragma once

#include "Scheduler.hpp"
#include "SubTimesliceDescriptor.hpp"
#include <atomic>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <cstdint>
#include <deque>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_def.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

struct IovecUcx {
  std::ptrdiff_t offset;
  std::size_t size;

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar & offset;
    ar & size;
  }
};

struct StComponentUcx {
  IovecUcx descriptor{};
  IovecUcx content{};
  bool is_missing_microslices = false;

  [[nodiscard]] uint64_t size() const { return descriptor.size + content.size; }

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar & descriptor;
    ar & content;
    ar & is_missing_microslices;
  }
};

struct StUcx {
  uint64_t start_time_ns;
  uint64_t duration_ns;
  bool is_incomplete;
  std::vector<StComponentUcx> components;

  [[nodiscard]] uint64_t size() const {
    uint64_t total_size = 0;
    for (const auto& component : components) {
      total_size += component.size();
    }
    return total_size;
  }

  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int /* version */) {
    ar & start_time_ns;
    ar & duration_ns;
    ar & is_incomplete;
    ar & components;
  }

  [[nodiscard]] std::string to_string() const {
    std::ostringstream oss;
    boost::archive::binary_oarchive oa(oss);
    oa << *this;
    return oss.str();
  }
};

// Announce subtimeslices to tssched and send them to tsbuilders
class StSender {
public:
  using StID = std::size_t;

  StSender(uint16_t listen_port,
           std::string_view tssched_address,
           boost::interprocess::managed_shared_memory* shm);
  ~StSender();
  StSender(const StSender&) = delete;
  StSender& operator=(const StSender&) = delete;

  void operator()();
  void stop() { stopped_ = true; }
  void announce_subtimeslice(StID id, const fles::SubTimesliceDescriptor& st);
  void retract_subtimeslice(StID id);
  bool try_receive_completion(StID* id);

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

  uint16_t listen_port_;
  std::string tssched_address_;
  uint16_t tssched_port_ = 13373;
  std::string sender_id_ = "StSender"; // TODO: set from hostname plus pid
  boost::interprocess::managed_shared_memory* shm_ = nullptr;

  std::atomic<bool> stopped_ = false;
  int queue_event_fd_ = -1;
  std::deque<std::pair<StID, fles::SubTimesliceDescriptor>>
      pending_announcements_;
  std::deque<std::size_t> pending_retractions_;
  std::mutex queue_mutex_;
  std::queue<std::size_t> completed_sts_;
  std::mutex completions_mutex_;
  int ucx_event_fd_ = -1;
  int epoll_fd_ = -1;

  std::unordered_map<StID, std::pair<std::string, std::vector<ucp_dt_iov>>>
      announced_sts_;
  std::unordered_map<ucs_status_ptr_t, StID> active_send_requests_;

  void put_completion(std::size_t id);

  ucp_context_h context_ = nullptr;
  ucp_worker_h worker_ = nullptr;
  ucp_listener_h listener_ = nullptr;
  std::vector<ucp_ep_h> connections_;

  ucp_ep_h tssched_ep_ = nullptr;
  bool tssched_connected_ = false;
  bool tssched_registered_ = false;
  void tssched_connect();
  void tssched_register();
  void tssched_disconnect(bool force = false);
  void try_tssched_connect();

  void ucx_listener_create();

  // Static callback functions
  static void ucp_listener_conn_callback(ucp_conn_request_h conn_request,
                                         void* arg);
  static void ucp_err_handler_cb(void* arg, ucp_ep_h ep, ucs_status_t status);
  static ucs_status_t
  ucp_am_recv_builder_request_st(void* arg,
                                 const void* header,
                                 size_t header_length,
                                 void* data,
                                 size_t length,
                                 const ucp_am_recv_param_t* param);
  static void ucp_am_recv_data_nbx_callback(void* request,
                                            ucs_status_t status,
                                            size_t length,
                                            void* user_data);
  static void
  ucp_send_nbx_callback(void* request, ucs_status_t status, void* user_data);

  void listener_conn(ucp_conn_request_h conn_request);
  void err_handler(ucp_ep_h ep, ucs_status_t status);
  ucs_status_t am_recv_builder_request_st(const void* header,
                                          size_t header_length,
                                          void* data,
                                          size_t length,
                                          const ucp_am_recv_param_t* param);
  void am_recv_data(void* request, ucs_status_t status, size_t length);
  void send_nbx_callback(void* request, ucs_status_t status);

  std::size_t handle_queues();

  static void
  tssched_err_handler_cb(void* arg, ucp_ep_h ep, ucs_status_t status);
  static ucs_status_t
  tssched_ucp_am_recv_sched_release_st(void* arg,
                                       const void* header,
                                       size_t header_length,
                                       void* data,
                                       size_t length,
                                       const ucp_am_recv_param_t* param);
  static void tssched_ucp_send_nbx_callback(void* request,
                                            ucs_status_t status,
                                            void* user_data);

  void tssched_err_handler(ucp_ep_h ep, ucs_status_t status);
  ucs_status_t
  tssched_am_recv_sched_release_st(const void* header,
                                   size_t header_length,
                                   void* data,
                                   size_t length,
                                   const ucp_am_recv_param_t* param);
  void tssched_send_nbx_callback(void* request, ucs_status_t status);

  void handle_announcement(StID id, const fles::SubTimesliceDescriptor& st_d);
  void handle_retraction(StID id);
  void tssched_send_announcement(StID id);
  void tssched_send_retraction(StID id);

  Scheduler scheduler_;
};
