// Copyright 2025 Jan de Cuveland

#include "StSender.hpp"
#include "SubTimesliceDescriptor.hpp"
#include "log.hpp"
#include "ucxutil.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <netdb.h>
#include <netinet/in.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_compat.h>
#include <ucs/type/status.h>

StSender::StSender(uint16_t listen_port,
                   std::string_view scheduler_address,
                   boost::interprocess::managed_shared_memory* shm)
    : listen_port_(listen_port), scheduler_address_(scheduler_address),
      shm_(shm) {
  // Initialize event handling
  queue_event_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (queue_event_fd_ == -1) {
    throw std::runtime_error("eventfd failed");
  }
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }

  // Add message queue's eventfd to epoll
  epoll_event ev{};
  ev.events = EPOLLIN | EPOLLET; // Edge-triggered
  ev.data.fd = queue_event_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, queue_event_fd_, &ev) == -1) {
    throw std::runtime_error("epoll_ctl failed for message queue");
  }
}

StSender::~StSender() {
  if (epoll_fd_ != -1) {
    close(epoll_fd_);
  }
  if (queue_event_fd_ != -1) {
    close(queue_event_fd_);
  }
}

// Public API methods

void StSender::announce_subtimeslice(StID id,
                                     const fles::SubTimesliceDescriptor& st) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_announcements_.emplace_back(id, st);
  }
  notify_queue_update();
}

void StSender::retract_subtimeslice(StID id) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    auto it = std::find_if(pending_announcements_.begin(),
                           pending_announcements_.end(),
                           [id](const auto& item) { return item.first == id; });
    if (it != pending_announcements_.end()) {
      pending_announcements_.erase(it);
      complete_subtimeslice(id);
      return;
    }
    pending_retractions_.emplace_back(id);
  }
  notify_queue_update();
}

bool StSender::try_receive_completion(StID* id) {
  std::lock_guard<std::mutex> lock(completions_mutex_);
  if (completed_sts_.empty()) {
    return false;
  }
  *id = completed_sts_.front();
  completed_sts_.pop();
  return true;
}

// Main operation loop

void StSender::operator()() {
  initialize_ucx();
  connect_to_scheduler_if_needed();
  create_listener();

  std::array<epoll_event, 1> events{};
  while (!stopped_) {
    if (ucp_worker_progress(worker_) != 0) {
      continue;
    }
    if (process_queues() > 0) {
      continue;
    }
    scheduler_.timer();

    if (!arm_worker_and_wait(events)) {
      break;
    }
  }

  cleanup();
}

// Network initialization/cleanup

void StSender::initialize_ucx() {
  if (context_ == nullptr) {
    ucx::util::ucx_init(context_, worker_);
    ucs_status_t status = ucp_worker_get_efd(worker_, &ucx_event_fd_);
    if (status != UCS_OK) {
      throw std::runtime_error("Failed to get UCP worker event fd: " +
                               std::string(ucs_status_string(status)));
    }
    L_(info) << "UCP context and worker initialized successfully";

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = ucx_event_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, ucx_event_fd_, &ev) == -1) {
      throw std::runtime_error("epoll_ctl failed for ucx event fd");
    }
  }
}

void StSender::create_listener() {
  if (listener_ != nullptr) {
    return;
  }

  struct sockaddr_in listen_addr {};
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  listen_addr.sin_port = htons(listen_port_);

  ucp_listener_params_t params{};
  params.field_mask = UCP_LISTENER_PARAM_FIELD_SOCK_ADDR |
                      UCP_LISTENER_PARAM_FIELD_CONN_HANDLER;
  params.sockaddr.addr = reinterpret_cast<const struct sockaddr*>(&listen_addr);
  params.sockaddr.addrlen = sizeof(listen_addr);
  params.conn_handler.cb = on_new_connection;
  params.conn_handler.arg = this;

  ucs_status_t status = ucp_listener_create(worker_, &params, &listener_);
  if (status != UCS_OK) {
    throw std::runtime_error("Failed to create UCP listener");
  }
  L_(info) << "Listening for connections on port " << listen_port_;
}

void StSender::cleanup() {
  if (listener_ != nullptr) {
    ucp_listener_destroy(listener_);
    listener_ = nullptr;
  }
  disconnect_from_scheduler();
  ucx::util::ucx_cleanup(context_, worker_);
}

// Scheduler connection management

void StSender::connect_to_scheduler() {
  if (scheduler_connected_) {
    return;
  }

  struct addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str = std::to_string(scheduler_port_);
  struct addrinfo* result;
  int err = getaddrinfo(scheduler_address_.c_str(), port_str.c_str(), &hints,
                        &result);
  if (err != 0) {
    L_(error) << "Failed to resolve hostname: " +
                     std::string(gai_strerror(err));
    return;
  }

  // Create endpoint
  ucp_ep_params_t ep_params{};
  ep_params.flags = UCP_EP_PARAMS_FLAGS_CLIENT_SERVER;
  ep_params.field_mask =
      UCP_EP_PARAM_FIELD_SOCK_ADDR | UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
      UCP_EP_PARAM_FIELD_ERR_HANDLER | UCP_EP_PARAM_FIELD_FLAGS;
  ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
  ep_params.err_handler.cb = on_scheduler_error;
  ep_params.err_handler.arg = this;

  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    ep_params.sockaddr.addr = rp->ai_addr;
    ep_params.sockaddr.addrlen = rp->ai_addrlen;

    ucs_status_t status = ucp_ep_create(worker_, &ep_params, &scheduler_ep_);
    if (status == UCS_OK) {
      L_(debug) << "UCX endpoint for scheduler created successfully";
      break;
    }
    L_(error) << "Failed to create UCX endpoint for scheduler: "
              << ucs_status_string(status);
  }
  freeaddrinfo(result);

  if (scheduler_ep_ == nullptr) {
    return;
  }

  scheduler_connected_ = true;
  L_(info) << "Connected to remote scheduler endpoint";

  register_scheduler_handlers();
  register_with_scheduler();
}

void StSender::register_scheduler_handlers() {
  ucp_am_handler_param_t param{};
  param.field_mask = UCP_AM_HANDLER_PARAM_FIELD_ID |
                     UCP_AM_HANDLER_PARAM_FIELD_CB |
                     UCP_AM_HANDLER_PARAM_FIELD_ARG;
  param.id = AM_SCHED_RELEASE_ST;
  param.cb = on_scheduler_release;
  param.arg = this;

  ucs_status_t status = ucp_worker_set_am_recv_handler(worker_, &param);
  if (status != UCS_OK) {
    L_(error) << "Failed to set active message receive handler: "
              << ucs_status_string(status);
    disconnect_from_scheduler(true);
    return;
  }
  L_(debug) << "Active message receive handler set for endpoint";
}

void StSender::register_with_scheduler() {
  send_active_message(scheduler_ep_, AM_SENDER_REGISTER, sender_id_.data(),
                      sender_id_.size(), nullptr, 0, on_scheduler_send_complete,
                      0);
  scheduler_registered_ = true;
}

void StSender::disconnect_from_scheduler(bool force) {
  scheduler_connected_ = false;
  scheduler_registered_ = false;

  if (scheduler_ep_ == nullptr) {
    return;
  }

  // Complete any announced subtimeslices
  for (const auto& [id, st] : announced_sts_) {
    L_(debug) << "Flushing announced SubTimeslice with ID: " << id;
    complete_subtimeslice(id);
  }
  announced_sts_.clear();

  // Close endpoint
  L_(debug) << "Requesting endpoint close to scheduler";
  ucp_request_param_t param{};
  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
  param.flags = force ? UCP_EP_CLOSE_MODE_FORCE : UCP_EP_CLOSE_MODE_FLUSH;
  ucs_status_ptr_t request = ucp_ep_close_nbx(scheduler_ep_, &param);

  wait_for_request_completion(request);
  scheduler_ep_ = nullptr;
  L_(debug) << "Disconnected from scheduler";
}

void StSender::connect_to_scheduler_if_needed() {
  constexpr auto interval = std::chrono::seconds(2);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  if (!scheduler_connected_ && !stopped_) {
    connect_to_scheduler();
  }

  scheduler_.add([this] { connect_to_scheduler_if_needed(); }, now + interval);
}

// Queue processing

void StSender::notify_queue_update() const {
  uint64_t value = 1;
  write(queue_event_fd_, &value, sizeof(value));
}

std::size_t StSender::process_queues() {
  std::deque<std::pair<StID, fles::SubTimesliceDescriptor>> announcements;
  std::deque<std::size_t> retractions;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (pending_announcements_.empty() && pending_retractions_.empty()) {
      return 0;
    }
    announcements.swap(pending_announcements_);
    retractions.swap(pending_retractions_);
  }

  if (!scheduler_registered_) {
    L_(trace) << "Scheduler not registered, skipping announcements";
    for (const auto& [id, st_d] : announcements) {
      complete_subtimeslice(id);
    }
    return announcements.size();
  }

  for (auto id : retractions) {
    process_retraction(id);
  }
  for (const auto& [id, st_d] : announcements) {
    process_announcement(id, st_d);
  }

  return announcements.size() + retractions.size();
}

void StSender::process_announcement(StID id,
                                    const fles::SubTimesliceDescriptor& st_d) {
  // Create and serialize subtimeslice
  StUcx st = create_subtimeslice_ucx(st_d);

  std::string serialized = st.to_string();
  std::vector<ucp_dt_iov> iov_vector = create_iov_vector(st, serialized);

  // Store for future use
  announced_sts_[id] = {std::move(serialized), std::move(iov_vector)};

  // Ensure that the first iov component points to the string data
  assert(announced_sts_[id].second.front().buffer ==
         announced_sts_[id].first.data());

  // Send announcement to scheduler
  send_announcement_to_scheduler(id);
}

void StSender::process_retraction(StID id) {
  auto it = announced_sts_.find(id);
  if (it != announced_sts_.end()) {
    L_(debug) << "Retracting SubTimeslice with ID: " << id;
    announced_sts_.erase(it);
    send_retraction_to_scheduler(id);
    complete_subtimeslice(id);
  } else {
    L_(warning) << "Attempted to retract unknown SubTimeslice ID: " << id;
  }
}

void StSender::complete_subtimeslice(StID id) {
  std::lock_guard<std::mutex> lock(completions_mutex_);
  completed_sts_.push(id);
}

// UCX message handling

bool StSender::arm_worker_and_wait(std::array<epoll_event, 1>& events) {
  ucs_status_t status = ucp_worker_arm(worker_);
  if (status == UCS_ERR_BUSY) {
    return true;
  }
  if (status != UCS_OK) {
    L_(fatal) << "Failed to arm UCP worker: " << ucs_status_string(status);
    return false;
  }

  int nfds = epoll_wait(epoll_fd_, events.data(), events.size(),
                        1000); // 1 second timeout
  if (nfds == -1) {
    if (errno == EINTR) {
      return true;
    }
    L_(fatal) << "epoll_wait failed: " << strerror(errno);
    return false;
  }
  return true;
}

void StSender::wait_for_request_completion(ucs_status_ptr_t& request) {
  if (request != nullptr && UCS_PTR_IS_PTR(request)) {
    ucs_status_t status;
    do {
      ucp_worker_progress(worker_);
      status = ucp_request_check_status(request);
    } while (status == UCS_INPROGRESS);

    ucp_request_free(request);
  }
}

ucs_status_ptr_t StSender::send_active_message(ucp_ep_h ep,
                                               unsigned id,
                                               const void* header,
                                               size_t header_length,
                                               const void* buffer,
                                               size_t count,
                                               ucp_send_nbx_callback_t callback,
                                               uint32_t flags) {
  ucp_request_param_t param{};
  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS | UCP_OP_ATTR_FIELD_CALLBACK |
                       UCP_OP_ATTR_FIELD_USER_DATA;
  param.flags = flags;
  param.cb.send = callback;
  param.user_data = this;

  ucs_status_ptr_t request =
      ucp_am_send_nbx(ep, id, header, header_length, buffer, count, &param);

  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Failed to send active message: "
              << ucs_status_string(UCS_PTR_STATUS(request));
  } else if (request == nullptr) {
    // Operation completed immediately
    L_(trace) << "Active message sent successfully";
  }

  return request;
}

// Connection callbacks

void StSender::handle_new_connection(ucp_conn_request_h conn_request) {
  L_(debug) << "New connection request received";

  // Get client address
  ucp_conn_request_attr_t attr{};
  attr.field_mask = UCP_CONN_REQUEST_ATTR_FIELD_CLIENT_ADDR;

  ucs_status_t status = ucp_conn_request_query(conn_request, &attr);
  if (status != UCS_OK) {
    L_(error) << "Failed to query connection request";
    ucp_listener_reject(listener_, conn_request);
    return;
  }

  // Create endpoint
  ucp_ep_h ep;
  ucp_ep_params_t ep_params{};
  ep_params.field_mask = UCP_EP_PARAM_FIELD_CONN_REQUEST |
                         UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
                         UCP_EP_PARAM_FIELD_ERR_HANDLER;
  ep_params.conn_request = conn_request;
  ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
  ep_params.err_handler.cb = on_endpoint_error;
  ep_params.err_handler.arg = this;

  status = ucp_ep_create(worker_, &ep_params, &ep);
  if (status != UCS_OK) {
    L_(error) << "Failed to create endpoint from connection request";
    return;
  }

  L_(debug) << "Accepted connection from "
            << inet_ntoa(
                   reinterpret_cast<const sockaddr_in*>(&attr.client_address)
                       ->sin_addr);

  connections_.push_back(ep);
  register_builder_message_handlers(ep);
}

void StSender::register_builder_message_handlers(ucp_ep_h ep) {
  ucp_am_handler_param_t param{};
  param.field_mask = UCP_AM_HANDLER_PARAM_FIELD_ID |
                     UCP_AM_HANDLER_PARAM_FIELD_CB |
                     UCP_AM_HANDLER_PARAM_FIELD_ARG;
  param.id = AM_BUILDER_REQUEST_ST;
  param.cb = on_builder_request;
  param.arg = this;

  ucs_status_t status = ucp_worker_set_am_recv_handler(worker_, &param);
  if (status != UCS_OK) {
    L_(error) << "Failed to set active message receive handler: "
              << ucs_status_string(status);
    close_endpoint(ep);
  } else {
    L_(debug) << "Active message receive handler set for endpoint";
  }
}

void StSender::close_endpoint(ucp_ep_h ep) {
  ucp_request_param_t param{};
  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
  param.flags = UCP_EP_CLOSE_MODE_FORCE;
  ucp_ep_close_nbx(ep, &param);
}

// Error handlers

void StSender::handle_endpoint_error(ucp_ep_h ep, ucs_status_t status) {
  L_(error) << "Error on UCX endpoint: " << ucs_status_string(status);

  auto it = std::find(connections_.begin(), connections_.end(), ep);
  if (it != connections_.end()) {
    connections_.erase(it);
    L_(info) << "Removed disconnected endpoint";
  } else {
    L_(warning) << "Received error for unknown endpoint";
  }
}

void StSender::handle_scheduler_error(ucp_ep_h ep, ucs_status_t status) {
  L_(error) << "Error on UCX endpoint to scheduler: "
            << ucs_status_string(status);

  if (scheduler_ep_ == ep) {
    scheduler_connected_ = false;
    scheduler_registered_ = false;
    scheduler_ep_ = nullptr;
    L_(info) << "Disconnected from scheduler";
  } else {
    L_(error) << "Received error for unknown scheduler endpoint";
  }
}

// Active message handlers

ucs_status_t
StSender::handle_builder_request(const void* header,
                                 size_t header_length,
                                 void* /* data */,
                                 size_t length,
                                 const ucp_am_recv_param_t* param) {
  L_(trace) << "Received builder request ST with header length "
            << header_length << " and data length " << length;

  if (header_length != sizeof(uint64_t) || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    L_(error) << "Invalid builder request received";
    return UCS_OK;
  }

  auto id = *static_cast<const uint64_t*>(header);
  send_subtimeslice_to_builder(id, param->reply_ep);
  return UCS_OK;
}

ucs_status_t
StSender::handle_scheduler_release(const void* header,
                                   size_t header_length,
                                   void* /* data */,
                                   size_t length,
                                   const ucp_am_recv_param_t* param) {
  L_(trace) << "Received scheduler release ST with header length "
            << header_length << " and data length " << length;

  if (header_length != sizeof(uint64_t) || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    L_(error) << "Invalid scheduler request received";
    return UCS_OK;
  }

  auto id = *static_cast<const uint64_t*>(header);
  auto it = announced_sts_.find(id);
  if (it != announced_sts_.end()) {
    L_(debug) << "Removing released SubTimeslice with ID: " << id;
    announced_sts_.erase(it);
    complete_subtimeslice(id);
  } else {
    L_(warning) << "Release for unknown SubTimeslice ID: " << id;
  }
  return UCS_OK;
}

// UCX static callbacks

void StSender::on_new_connection(ucp_conn_request_h conn_request, void* arg) {
  static_cast<StSender*>(arg)->handle_new_connection(conn_request);
}

void StSender::on_endpoint_error(void* arg, ucp_ep_h ep, ucs_status_t status) {
  static_cast<StSender*>(arg)->handle_endpoint_error(ep, status);
}

void StSender::on_scheduler_error(void* arg, ucp_ep_h ep, ucs_status_t status) {
  static_cast<StSender*>(arg)->handle_scheduler_error(ep, status);
}

ucs_status_t StSender::on_builder_request(void* arg,
                                          const void* header,
                                          size_t header_length,
                                          void* data,
                                          size_t length,
                                          const ucp_am_recv_param_t* param) {
  return static_cast<StSender*>(arg)->handle_builder_request(
      header, header_length, data, length, param);
}

ucs_status_t StSender::on_scheduler_release(void* arg,
                                            const void* header,
                                            size_t header_length,
                                            void* data,
                                            size_t length,
                                            const ucp_am_recv_param_t* param) {
  return static_cast<StSender*>(arg)->handle_scheduler_release(
      header, header_length, data, length, param);
}

void StSender::on_builder_send_complete(void* request,
                                        ucs_status_t status,
                                        void* user_data) {
  static_cast<StSender*>(user_data)->handle_builder_send_complete(request,
                                                                  status);
}

void StSender::on_scheduler_send_complete(void* request,
                                          ucs_status_t status,
                                          void* /* user_data */) {
  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Send operation failed: " << ucs_status_string(status);
  } else if (status != UCS_OK) {
    L_(error) << "Send operation completed with status: "
              << ucs_status_string(status);
  } else {
    L_(trace) << "Send operation completed successfully";
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
}

// Helper methods

void StSender::send_subtimeslice_to_builder(StID id, ucp_ep_h ep) {
  // Prepare send parameters
  ucp_request_param_t req_param{};
  req_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_FLAGS | UCP_OP_ATTR_FIELD_CALLBACK |
      UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FIELD_DATATYPE;
  req_param.flags = UCP_AM_SEND_FLAG_COPY_HEADER | UCP_AM_SEND_FLAG_RNDV;
  req_param.cb.send = on_builder_send_complete;
  req_param.user_data = this;
  req_param.datatype = ucp_dt_make_iov();

  // Prepare header data
  std::array<uint64_t, 3> hdr{id, 0, 0};
  void const* buffer = nullptr;
  std::size_t count = 0;

  auto it = announced_sts_.find(id);
  if (it != announced_sts_.end()) {
    const auto& [st_str, iov_vector] = it->second;
    uint64_t desc_size = st_str.size();
    uint64_t content_size = 0;
    for (std::size_t i = 1; i < iov_vector.size(); ++i) {
      content_size += iov_vector[i].length;
    }
    hdr[1] = desc_size;
    hdr[2] = content_size;
    buffer = iov_vector.data();
    count = iov_vector.size();
  } else {
    L_(warning) << "SubTimeslice with ID " << id << " not found";
  }

  // Send the data
  ucs_status_ptr_t request =
      ucp_am_send_nbx(ep, AM_SENDER_SEND_ST, hdr.data(), sizeof(hdr), buffer,
                      count, &req_param);

  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Failed to send active message: "
              << ucs_status_string(UCS_PTR_STATUS(request));
    return;
  }

  if (request == nullptr) {
    // Operation completed immediately
    L_(trace) << "Active message sent successfully";
    ucp_request_free(request);
    complete_subtimeslice(id);
    return;
  }

  active_send_requests_[request] = id;
  announced_sts_.erase(it);
}

void StSender::handle_builder_send_complete(void* request,
                                            ucs_status_t status) {
  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Send operation failed: " << ucs_status_string(status);
  } else if (status != UCS_OK) {
    L_(error) << "Send operation completed with status: "
              << ucs_status_string(status);
  } else {
    L_(trace) << "Send operation completed successfully";
  }

  StID id = active_send_requests_[request];
  complete_subtimeslice(id);
  active_send_requests_.erase(request);
  ucp_request_free(request);
}

void StSender::send_announcement_to_scheduler(StID id) {
  auto it = announced_sts_.find(id);
  if (it == announced_sts_.end()) {
    return;
  }

  const auto& [st_str, iov_vector] = it->second;
  assert(iov_vector.size() > 0);
  uint64_t desc_size = st_str.size();
  uint64_t content_size = 0;
  for (std::size_t i = 1; i < iov_vector.size(); ++i) {
    content_size += iov_vector[i].length;
  }
  std::array<uint64_t, 3> hdr{id, desc_size, content_size};

  send_active_message(scheduler_ep_, AM_SENDER_ANNOUNCE_ST, hdr.data(),
                      sizeof(hdr), iov_vector[0].buffer, iov_vector[0].length,
                      on_scheduler_send_complete, UCP_AM_SEND_FLAG_COPY_HEADER);
}

void StSender::send_retraction_to_scheduler(StID id) {
  std::array<uint64_t, 1> hdr{id};
  send_active_message(scheduler_ep_, AM_SENDER_RETRACT_ST, hdr.data(),
                      sizeof(hdr), nullptr, 0, on_scheduler_send_complete,
                      UCP_AM_SEND_FLAG_COPY_HEADER);
}

StUcx StSender::create_subtimeslice_ucx(
    const fles::SubTimesliceDescriptor& st_d) {
  StUcx st;
  st.start_time_ns = st_d.start_time_ns;
  st.duration_ns = st_d.duration_ns;
  st.is_incomplete = st_d.is_incomplete;

  std::ptrdiff_t offset = 0;
  for (const auto& c : st_d.components) {
    std::size_t descriptors_size = 0;
    for (auto descriptor : c.descriptors) {
      descriptors_size += descriptor.size;
    }
    const IovecUcx descriptor = {offset, descriptors_size};
    offset += descriptors_size;

    std::size_t content_size = 0;
    for (auto content : c.contents) {
      content_size += content.size;
    }
    const IovecUcx content = {offset, content_size};
    offset += content_size;

    st.components.push_back({descriptor, content, c.is_missing_microslices});
  }

  return st;
}

std::vector<ucp_dt_iov>
StSender::create_iov_vector(const StUcx& st, const std::string& serialized) {

  std::vector<ucp_dt_iov> iov_vector;

  // Add serialized descriptor at the beginning
  void* buffer = const_cast<char*>(serialized.data());
  std::size_t length = serialized.size();
  iov_vector.push_back({buffer, length});

  // Add component data from shared memory
  for (const auto& c : st.components) {
    // Add descriptors
    void* desc_ptr = shm_->get_address_from_handle(c.descriptor.offset);
    iov_vector.push_back({desc_ptr, c.descriptor.size});

    // Add content
    void* content_ptr = shm_->get_address_from_handle(c.content.offset);
    iov_vector.push_back({content_ptr, c.content.size});
  }

  return iov_vector;
}
