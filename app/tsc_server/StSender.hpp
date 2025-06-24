// Copyright 2025 Jan de Cuveland
#pragma once

#include "Scheduler.hpp"
#include "SubTimesliceDescriptor.hpp"
#include <array>
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
#include <sys/epoll.h>
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
           std::string_view scheduler_address,
           boost::interprocess::managed_shared_memory* shm);
  ~StSender();
  StSender(const StSender&) = delete;
  StSender& operator=(const StSender&) = delete;

  // Public API methods
  void announce_subtimeslice(StID id, const fles::SubTimesliceDescriptor& st);
  void retract_subtimeslice(StID id);
  bool try_receive_completion(StID* id);

  // Main operation loop
  void operator()();
  void stop() { stopped_ = true; }

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

  Scheduler scheduler_;

  uint16_t listen_port_;
  std::string scheduler_address_;
  uint16_t scheduler_port_ = 13373;
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

  ucp_context_h context_ = nullptr;
  ucp_worker_h worker_ = nullptr;
  ucp_listener_h listener_ = nullptr;
  std::vector<ucp_ep_h> connections_;

  ucp_ep_h scheduler_ep_ = nullptr;
  bool scheduler_connected_ = false;
  bool scheduler_registered_ = false;

  // Network initialization/cleanup
  void initialize_ucx();
  void create_listener();
  void cleanup();

  // Scheduler connection management
  void connect_to_scheduler();
  void register_scheduler_handlers();
  void register_with_scheduler();
  void disconnect_from_scheduler(bool force = false);
  void connect_to_scheduler_if_needed();

  // Queue processing
  void notify_queue_update() const;
  std::size_t process_queues();
  void process_announcement(StID id, const fles::SubTimesliceDescriptor& st_d);
  void process_retraction(StID id);
  void complete_subtimeslice(std::size_t id);

  // UCX message handling
  bool arm_worker_and_wait(std::array<epoll_event, 1>& events);
  void wait_for_request_completion(ucs_status_ptr_t& request);
  ucs_status_ptr_t send_active_message(ucp_ep_h ep,
                                       unsigned id,
                                       const void* header,
                                       size_t header_length,
                                       const void* buffer,
                                       size_t count,
                                       ucp_send_nbx_callback_t callback,
                                       uint32_t flags);

  // Connection callbacks
  void handle_new_connection(ucp_conn_request_h conn_request);
  void register_builder_message_handlers(ucp_ep_h ep);
  void close_endpoint(ucp_ep_h ep);

  // Error handlers
  void handle_endpoint_error(ucp_ep_h ep, ucs_status_t status);
  void handle_scheduler_error(ucp_ep_h ep, ucs_status_t status);
  ucs_status_t handle_builder_request(const void* header,
                                      size_t header_length,
                                      void* data,
                                      size_t length,
                                      const ucp_am_recv_param_t* param);
  ucs_status_t handle_scheduler_release(const void* header,
                                        size_t header_length,
                                        void* data,
                                        size_t length,
                                        const ucp_am_recv_param_t* param);

  // UCX static callbacks
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
  static void on_scheduler_send_complete(void* request,
                                         ucs_status_t status,
                                         void* user_data);

  // Helper methods
  void send_subtimeslice_to_builder(StID id, ucp_ep_h ep);
  void handle_builder_send_complete(void* request, ucs_status_t status);
  void send_announcement_to_scheduler(StID id);
  void send_retraction_to_scheduler(StID id);
  StUcx create_subtimeslice_ucx(const fles::SubTimesliceDescriptor& st_d);
  std::vector<ucp_dt_iov> create_iov_vector(const StUcx& st,
                                            const std::string& serialized);
};
