// Copyright 2025 Jan de Cuveland

#include "StSender.hpp"
#include "log.hpp"
#include "ucxutil.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <netdb.h>
#include <netinet/in.h>
#include <sched.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_compat.h>
#include <ucs/type/status.h>

StSender::StSender(uint16_t listen_port,
                   std::string_view tssched_address,
                   boost::interprocess::managed_shared_memory* shm)
    : listen_port_(listen_port), tssched_address_(tssched_address), shm_(shm) {
  queue_event_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (queue_event_fd_ == -1) {
    throw std::runtime_error("eventfd failed");
  }
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }
  // Add the message queue's eventfd to epoll
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

void StSender::announce_subtimeslice(StID id,
                                     const fles::SubTimesliceDescriptor& st) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_announcements_.emplace_back(id, st);
  }
  uint64_t value = 1;
  write(queue_event_fd_, &value, sizeof(value));
}

void StSender::retract_subtimeslice(StID id) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    auto it = std::find_if(pending_announcements_.begin(),
                           pending_announcements_.end(),
                           [id](const auto& item) { return item.first == id; });
    if (it != pending_announcements_.end()) {
      pending_announcements_.erase(it);
      put_completion(id);
      return;
    }
    pending_retractions_.emplace_back(id);
  }
  uint64_t value = 1;
  write(queue_event_fd_, &value, sizeof(value));
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

void StSender::tssched_connect() {
  assert(!tssched_connected_);
  struct addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str = std::to_string(tssched_port_);

  struct addrinfo* result;
  int err =
      getaddrinfo(tssched_address_.c_str(), port_str.c_str(), &hints, &result);
  if (err != 0) {
    L_(error) << "Failed to resolve hostname: " +
                     std::string(gai_strerror(err));
  }

  ucs_status_t status;
  ucp_ep_params_t ep_params{};
  ep_params.flags = UCP_EP_PARAMS_FLAGS_CLIENT_SERVER;
  ep_params.field_mask =
      UCP_EP_PARAM_FIELD_SOCK_ADDR | UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
      UCP_EP_PARAM_FIELD_ERR_HANDLER | UCP_EP_PARAM_FIELD_FLAGS;
  ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
  ep_params.err_handler.cb = tssched_err_handler_cb;
  ep_params.err_handler.arg = this;

  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    ep_params.sockaddr.addr = rp->ai_addr;
    ep_params.sockaddr.addrlen = rp->ai_addrlen;

    status = ucp_ep_create(worker_, &ep_params, &tssched_ep_);
    if (status != UCS_OK) {
      L_(error) << "Failed to create UCX endpoint for sched: "
                << ucs_status_string(status);
    } else {
      L_(debug) << "UCX endpoint for sched created successfully";
      break;
    }
  }
  freeaddrinfo(result);

  if (status != UCS_OK) {
    L_(error) << "Failed to create UCX endpoint for sched: "
              << ucs_status_string(status);
    return;
  }
  tssched_connected_ = true;
  L_(info) << "Connected to remote sched endpoint" << std::endl;

  ucp_am_handler_param_t param{};
  param.field_mask = UCP_AM_HANDLER_PARAM_FIELD_ID |
                     UCP_AM_HANDLER_PARAM_FIELD_CB |
                     UCP_AM_HANDLER_PARAM_FIELD_ARG;
  param.id = AM_SCHED_RELEASE_ST;
  param.cb = tssched_ucp_am_recv_sched_release_st;
  param.arg = this;
  status = ucp_worker_set_am_recv_handler(worker_, &param);
  if (status != UCS_OK) {
    L_(error) << "Failed to set active message receive handler: "
              << ucs_status_string(status);
    tssched_disconnect(true);
    return;
  }
  L_(debug) << "Active message receive handler set for endpoint";

  tssched_register();
}

void StSender::tssched_register() {
  ucp_request_param_t param{};
  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS | UCP_OP_ATTR_FIELD_CALLBACK |
                       UCP_OP_ATTR_FIELD_USER_DATA;
  param.flags = UCP_AM_SEND_FLAG_COPY_HEADER;
  param.cb.send = tssched_ucp_send_nbx_callback;
  param.user_data = this;

  ucs_status_ptr_t request =
      ucp_am_send_nbx(tssched_ep_, AM_SENDER_REGISTER, sender_id_.data(),
                      sender_id_.size(), nullptr, 0, &param);

  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Failed to send registration message: "
              << ucs_status_string(UCS_PTR_STATUS(request));
    return;
  }
  if (request == nullptr) {
    // Operation completed immediately
    L_(trace) << "Registration message sent successfully";
    ucp_request_free(request);
  }
  tssched_registered_ = true;
}

void StSender::tssched_disconnect(bool force) {
  tssched_connected_ = false;
  tssched_registered_ = false;

  if (tssched_ep_ == nullptr) {
    return;
  }

  for (const auto& [id, st] : announced_sts_) {
    L_(debug) << "Flushing announced SubTimeslice with ID: " << id;
    put_completion(id);
  }
  announced_sts_.clear();

  L_(debug) << "Requesting endpoint close to tssched";
  ucp_request_param_t param{};
  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
  param.flags = force ? UCP_EP_CLOSE_MODE_FORCE : UCP_EP_CLOSE_MODE_FLUSH;
  ucs_status_ptr_t request = ucp_ep_close_nbx(tssched_ep_, &param);

  if (request != nullptr && UCS_PTR_IS_PTR(request)) {
    ucs_status_t status;
    do {
      ucp_worker_progress(worker_);
      status = ucp_request_check_status(request);
    } while (status == UCS_INPROGRESS);

    ucp_request_free(request);
  }

  tssched_ep_ = nullptr;

  L_(debug) << "Disconnected from tssched";
}

void StSender::operator()() {
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
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, queue_event_fd_, &ev) == -1) {
      throw std::runtime_error("epoll_ctl failed for ucx event fd");
    }
  }
  tssched_connect();
  if (listener_ == nullptr) {
    ucx_listener_create();
  }
  std::array<epoll_event, 1> events{};
  while (!stopped_) {
    if (ucp_worker_progress(worker_) != 0) {
      continue;
    }
    if (handle_queues() > 0) {
      continue;
    }
    scheduler_.timer();
    ucs_status_t status = ucp_worker_arm(worker_);
    if (status == UCS_ERR_BUSY) {
      continue;
    }
    if (status != UCS_OK) {
      L_(fatal) << "Failed to arm UCP worker: " << ucs_status_string(status);
      break;
    }

    int nfds = epoll_wait(epoll_fd_, events.data(), events.size(),
                          1000); // 1 second timeout
    if (nfds == -1) {
      if (errno == EINTR) {
        continue;
      }
      L_(fatal) << "epoll_wait failed: " << strerror(errno);
      break;
    }
  }
  if (listener_ != nullptr) {
    ucp_listener_destroy(listener_);
    listener_ = nullptr;
  }
  tssched_disconnect();
  ucx::util::ucx_cleanup(context_, worker_);
}

void StSender::try_tssched_connect() {
  constexpr auto interval = std::chrono::seconds(2);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  if (!tssched_connected_ && !stopped_) {
    tssched_connect();
  }

  scheduler_.add([this] { try_tssched_connect(); }, now + interval);
}

void StSender::ucx_listener_create() {
  if (listener_ != nullptr) {
    throw std::runtime_error("UCP listener already created");
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
  params.conn_handler.cb = ucp_listener_conn_callback;
  params.conn_handler.arg = this;

  ucs_status_t status = ucp_listener_create(worker_, &params, &listener_);
  if (status != UCS_OK) {
    throw std::runtime_error("Failed to create UCP listener");
  }
  L_(info) << "Listening for connections on port " << listen_port_;
}

void StSender::put_completion(StID id) {
  std::lock_guard<std::mutex> lock(completions_mutex_);
  completed_sts_.push(id);
}

void StSender::listener_conn(ucp_conn_request_h conn_request) {
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

  ucp_ep_h ep;
  ucp_ep_params_t ep_params;
  memset(&ep_params, 0, sizeof(ep_params));

  ep_params.field_mask = UCP_EP_PARAM_FIELD_CONN_REQUEST |
                         UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
                         UCP_EP_PARAM_FIELD_ERR_HANDLER;
  ep_params.conn_request = conn_request;
  ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
  ep_params.err_handler.cb = ucp_err_handler_cb;
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

  ucp_am_handler_param_t param{};
  param.field_mask = UCP_AM_HANDLER_PARAM_FIELD_ID |
                     UCP_AM_HANDLER_PARAM_FIELD_CB |
                     UCP_AM_HANDLER_PARAM_FIELD_ARG;
  param.id = AM_BUILDER_REQUEST_ST;
  param.cb = ucp_am_recv_builder_request_st;
  param.arg = this;
  status = ucp_worker_set_am_recv_handler(worker_, &param);
  if (status != UCS_OK) {
    L_(error) << "Failed to set active message receive handler: "
              << ucs_status_string(status);
    ucp_request_param_t close_param{};
    close_param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
    close_param.flags = UCP_EP_CLOSE_MODE_FORCE;
    ucp_ep_close_nbx(ep, &close_param);
    return;
  }
  L_(debug) << "Active message receive handler set for endpoint";
}

void StSender::err_handler(ucp_ep_h ep, ucs_status_t status) {
  L_(error) << "Error on UCX endpoint: " << ucs_status_string(status);

  auto it = std::find(connections_.begin(), connections_.end(), ep);
  if (it != connections_.end()) {
    connections_.erase(it);
    L_(info) << "Removed disconnected endpoint";
  } else {
    L_(warning) << "Received error for unknown endpoint";
  }
}

ucs_status_t
StSender::am_recv_builder_request_st(const void* header,
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

  // Send subtimeslice as an active message
  ucp_request_param_t req_param{};
  req_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_FLAGS | UCP_OP_ATTR_FIELD_CALLBACK |
      UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FIELD_DATATYPE;
  req_param.flags = UCP_AM_SEND_FLAG_COPY_HEADER | UCP_AM_SEND_FLAG_RNDV;
  req_param.cb.send = ucp_send_nbx_callback;
  req_param.user_data = this;
  req_param.datatype = ucp_dt_make_iov();

  // Prepare the data to send
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

  ucs_status_ptr_t request =
      ucp_am_send_nbx(param->reply_ep, AM_SENDER_SEND_ST, hdr.data(),
                      sizeof(hdr), buffer, count, &req_param);

  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Failed to send active message: "
              << ucs_status_string(UCS_PTR_STATUS(request));
    return UCS_OK;
  }
  if (request == nullptr) {
    // Operation completed immediately
    L_(trace) << "Active message sent successfully";
    ucp_request_free(request);
    put_completion(id);
  }

  active_send_requests_.emplace(request, id);
  announced_sts_.erase(it);
  return UCS_OK;
}

void StSender::am_recv_data(void* /* request */,
                            ucs_status_t /* status */,
                            size_t /* length */) {
  // ... unused for now ...
}

// send_st_completed
void StSender::send_nbx_callback(void* request, ucs_status_t status) {
  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Send operation failed: " << ucs_status_string(status);
    active_send_requests_.erase(request);
    ucp_request_free(request);
    return;
  }
  if (status == UCS_OK) {
    L_(trace) << "Send operation completed successfully";
  } else {
    L_(error) << "Send operation completed with status: "
              << ucs_status_string(status);
  }
  StID id = active_send_requests_[request];
  put_completion(id);
  active_send_requests_.erase(request);
  ucp_request_free(request);
}

void StSender::handle_retraction(StID id) {
  auto it = announced_sts_.find(id);
  if (it != announced_sts_.end()) {
    L_(debug) << "Retracting SubTimeslice with ID: " << id;
    announced_sts_.erase(it);
    tssched_send_retraction(id);
    put_completion(id);
  } else {
    L_(warning) << "Attempted to retract unknown SubTimeslice ID: " << id;
  }
}

void StSender::tssched_send_announcement(StID id) {
  // Send subtimeslice announcement as an active message
  ucp_request_param_t req_param{};
  req_param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS |
                           UCP_OP_ATTR_FIELD_CALLBACK |
                           UCP_OP_ATTR_FIELD_USER_DATA;
  req_param.flags = UCP_AM_SEND_FLAG_COPY_HEADER;
  req_param.cb.send = tssched_ucp_send_nbx_callback;
  req_param.user_data = this;

  // Prepare the data to send
  auto it = announced_sts_.find(id);
  assert(it != announced_sts_.end());

  const auto& [st_str, iov_vector] = it->second;
  assert(iov_vector.size() > 0);
  uint64_t desc_size = st_str.size();
  uint64_t content_size = 0;
  for (std::size_t i = 1; i < iov_vector.size(); ++i) {
    content_size += iov_vector[i].length;
  }
  std::array<uint64_t, 3> hdr{id, desc_size, content_size};

  ucs_status_ptr_t request = ucp_am_send_nbx(
      tssched_ep_, AM_SENDER_ANNOUNCE_ST, hdr.data(), sizeof(hdr),
      iov_vector[0].buffer, iov_vector[0].length, &req_param);

  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Failed to send active message: "
              << ucs_status_string(UCS_PTR_STATUS(request));
    return;
  }
  if (request == nullptr) {
    // Operation completed immediately
    L_(trace) << "Active message sent successfully";
    ucp_request_free(request);
  }
}

void StSender::tssched_send_retraction(StID id) {
  // Send subtimeslice retraction as an active message
  ucp_request_param_t req_param{};
  req_param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS |
                           UCP_OP_ATTR_FIELD_CALLBACK |
                           UCP_OP_ATTR_FIELD_USER_DATA;
  req_param.flags = UCP_AM_SEND_FLAG_COPY_HEADER;
  req_param.cb.send = tssched_ucp_send_nbx_callback;
  req_param.user_data = this;

  std::array<uint64_t, 1> hdr{id};

  ucs_status_ptr_t request =
      ucp_am_send_nbx(tssched_ep_, AM_SENDER_RETRACT_ST, hdr.data(),
                      sizeof(hdr), nullptr, 0, &req_param);

  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Failed to send active message: "
              << ucs_status_string(UCS_PTR_STATUS(request));
    return;
  }
  if (request == nullptr) {
    // Operation completed immediately
    L_(trace) << "Active message sent successfully";
    ucp_request_free(request);
  }
}

void StSender::handle_announcement(StID id,
                                   const fles::SubTimesliceDescriptor& st_d) {
  StUcx st;
  std::vector<ucp_dt_iov> iov_vector;
  st.start_time_ns = st_d.start_time_ns;
  st.duration_ns = st_d.duration_ns;
  st.is_incomplete = st_d.is_incomplete;
  std::ptrdiff_t offset = 0;
  for (const auto& c : st_d.components) {
    std::size_t descriptors_size = 0;
    for (auto descriptor : c.descriptors) {
      descriptors_size += descriptor.size;
      void* ptr = shm_->get_address_from_handle(descriptor.offset);
      iov_vector.push_back({ptr, descriptor.size});
    }
    const IovecUcx descriptor = {offset, descriptors_size};
    offset += descriptors_size;
    std::size_t content_size = 0;
    for (auto content : c.contents) {
      content_size += content.size;
      void* ptr = shm_->get_address_from_handle(content.offset);
      iov_vector.push_back({ptr, content.size});
    }
    const IovecUcx content = {offset, content_size};
    offset += content_size;
    st.components.push_back({descriptor, content, c.is_missing_microslices});
  }

  std::string st_str = st.to_string();

  // Insert the serialized descriptor at the beginning
  iov_vector.insert(iov_vector.begin(), {st_str.data(), st_str.size()});
  announced_sts_[id] = {std::move(st_str), std::move(iov_vector)};
  // Ensure that the first iov component points to the string data
  assert(announced_sts_[id].second.front().buffer ==
         announced_sts_[id].first.data());
  tssched_send_announcement(id);
}

std::size_t StSender::handle_queues() {
  std::deque<std::pair<StID, fles::SubTimesliceDescriptor>> announcements;
  std::deque<std::size_t> retractions;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (announcements.empty() && retractions.empty()) {
      return 0;
    }
    announcements.swap(pending_announcements_);
    retractions.swap(pending_retractions_);
  }
  if (!tssched_registered_) {
    L_(trace) << "TSSched not registered, skipping announcements";
    for (const auto& [id, st_d] : announcements) {
      put_completion(id);
    }
    return announcements.size();
  }
  for (auto id : retractions) {
    handle_retraction(id);
  }
  for (const auto& [id, st_d] : announcements) {
    handle_announcement(id, st_d);
  }
  return announcements.size() + retractions.size();
}

void StSender::ucp_listener_conn_callback(ucp_conn_request_h conn_request,
                                          void* arg) {
  static_cast<StSender*>(arg)->listener_conn(conn_request);
}

void StSender::ucp_err_handler_cb(void* arg, ucp_ep_h ep, ucs_status_t status) {
  static_cast<StSender*>(arg)->err_handler(ep, status);
}

ucs_status_t
StSender::ucp_am_recv_builder_request_st(void* arg,
                                         const void* header,
                                         size_t header_length,
                                         void* data,
                                         size_t length,
                                         const ucp_am_recv_param_t* param) {
  return static_cast<StSender*>(arg)->am_recv_builder_request_st(
      header, header_length, data, length, param);
}

void StSender::ucp_am_recv_data_nbx_callback(void* request,
                                             ucs_status_t status,
                                             size_t length,
                                             void* user_data) {
  static_cast<StSender*>(user_data)->am_recv_data(request, status, length);
}

void StSender::ucp_send_nbx_callback(void* request,
                                     ucs_status_t status,
                                     void* user_data) {
  static_cast<StSender*>(user_data)->send_nbx_callback(request, status);
}

void StSender::tssched_err_handler_cb(void* arg,
                                      ucp_ep_h ep,
                                      ucs_status_t status) {
  static_cast<StSender*>(arg)->tssched_err_handler(ep, status);
}

ucs_status_t StSender::tssched_ucp_am_recv_sched_release_st(
    void* arg,
    const void* header,
    size_t header_length,
    void* data,
    size_t length,
    const ucp_am_recv_param_t* param) {
  return static_cast<StSender*>(arg)->tssched_am_recv_sched_release_st(
      header, header_length, data, length, param);
}

void StSender::tssched_ucp_send_nbx_callback(void* request,
                                             ucs_status_t status,
                                             void* user_data) {
  static_cast<StSender*>(user_data)->tssched_send_nbx_callback(request, status);
}

void StSender::tssched_err_handler(ucp_ep_h ep, ucs_status_t status) {
  L_(error) << "Error on UCX endpoint to tssched: "
            << ucs_status_string(status);

  if (tssched_ep_ != nullptr && tssched_ep_ == ep) {
    tssched_connected_ = false;
    tssched_registered_ = false;
    tssched_ep_ = nullptr;
    L_(info) << "Disconnected from tssched";
  } else {
    L_(error) << "Received error for unknown tssched endpoint";
  }
}

ucs_status_t
StSender::tssched_am_recv_sched_release_st(const void* header,
                                           size_t header_length,
                                           void* /* data */,
                                           size_t length,
                                           const ucp_am_recv_param_t* param) {
  L_(trace) << "Received tssched release ST with header length "
            << header_length << " and data length " << length;
  if (header_length != sizeof(uint64_t) || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    L_(error) << "Invalid tssched request received";
    return UCS_OK;
  }
  auto id = *static_cast<const uint64_t*>(header);
  auto it = announced_sts_.find(id);
  if (it != announced_sts_.end()) {
    L_(debug) << "Retracting SubTimeslice with ID: " << id;
    announced_sts_.erase(it);
    put_completion(id);
  } else {
    L_(warning) << "Attempted to retract unknown SubTimeslice ID: " << id;
  }
  return UCS_OK;
}

void StSender::tssched_send_nbx_callback(void* request, ucs_status_t status) {
  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Send operation failed: " << ucs_status_string(status);
    active_send_requests_.erase(request);
    ucp_request_free(request);
    return;
  }
  if (status == UCS_OK) {
    L_(trace) << "Send operation completed successfully";
  } else {
    L_(error) << "Send operation completed with status: "
              << ucs_status_string(status);
  }
  ucp_request_free(request);
}
