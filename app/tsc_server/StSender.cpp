// Copyright 2025 Jan de Cuveland

#include "StSender.hpp"
#include "System.hpp"
#include "TsbProtocol.hpp"
#include "log.hpp"
#include "monitoring/System.hpp"
#include "ucxutil.hpp"
#include <arpa/inet.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <sched.h>
#include <span>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_compat.h>
#include <ucs/type/status.h>

StSender::StSender(std::string_view scheduler_address, uint16_t listen_port)
    : scheduler_address_(scheduler_address), listen_port_(listen_port) {
  sender_id_ =
      fles::system::current_hostname() + ":" + std::to_string(listen_port_);

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

  // Start the worker thread
  worker_thread_ = std::jthread([this](std::stop_token st) { (*this)(st); });
}

StSender::~StSender() {
  worker_thread_.request_stop();

  if (epoll_fd_ != -1) {
    close(epoll_fd_);
  }
  if (queue_event_fd_ != -1) {
    close(queue_event_fd_);
  }
}

// Public API methods

void StSender::announce_subtimeslice(TsID id, const SubTimesliceHandle& st) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_announcements_.emplace_back(id, st);
  }
  notify_queue_update();
}

void StSender::retract_subtimeslice(TsID id) {
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

std::optional<TsID> StSender::try_receive_completion() {
  std::lock_guard<std::mutex> lock(completions_mutex_);
  if (completed_.empty()) {
    return std::nullopt;
  }
  TsID id = completed_.front();
  completed_.pop();
  return id;
}

// Main operation loop

void StSender::operator()(std::stop_token stop_token) {
  cbm::system::set_thread_name("StSender");

  if (!ucx::util::init(context_, worker_, epoll_fd_)) {
    ERROR("Failed to initialize UCX");
    return;
  }
  if (!ucx::util::set_receive_handler(worker_, AM_SCHED_RELEASE_ST,
                                      on_scheduler_release, this) ||
      !ucx::util::set_receive_handler(worker_, AM_BUILDER_REQUEST_ST,
                                      on_builder_request, this)) {
    ERROR("Failed to register receive handlers");
    return;
  }
  connect_to_scheduler_if_needed();
  if (!ucx::util::create_listener(worker_, listener_, listen_port_,
                                  on_new_connection, this)) {
    ERROR("Failed to create UCX listener");
    return;
  }

  while (!stop_token.stop_requested()) {
    if (ucp_worker_progress(worker_) != 0) {
      continue;
    }
    if (process_queues() > 0) {
      continue;
    }
    tasks_.timer();

    if (!ucx::util::arm_worker_and_wait(worker_, epoll_fd_)) {
      break;
    }
  }

  if (listener_ != nullptr) {
    ucp_listener_destroy(listener_);
    listener_ = nullptr;
  }
  disconnect_from_scheduler();
  ucx::util::cleanup(context_, worker_);
}

// Scheduler connection management

void StSender::connect_to_scheduler_if_needed() {
  constexpr auto interval = std::chrono::seconds(2);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  if (!scheduler_connecting_ &&
      !worker_thread_.get_stop_token().stop_requested()) {
    connect_to_scheduler();
  }

  tasks_.add([this] { connect_to_scheduler_if_needed(); }, now + interval);
}

void StSender::connect_to_scheduler() {
  if (scheduler_connecting_ || scheduler_connected_) {
    return;
  }

  auto [address, port] =
      ucx::util::parse_address(scheduler_address_, DEFAULT_SCHEDULER_PORT);
  auto ep =
      ucx::util::connect(worker_, address, port, on_scheduler_error, this);
  if (!ep) {
    ERROR("Failed to connect to scheduler at {}:{}", address, port);
    return;
  }

  scheduler_ep_ = *ep;

  if (!register_with_scheduler()) {
    ERROR("Failed to register with scheduler");
    disconnect_from_scheduler(true);
    return;
  }

  scheduler_connecting_ = true;
}

void StSender::handle_scheduler_error(ucp_ep_h ep, ucs_status_t status) {
  if (ep != scheduler_ep_) {
    ERROR("Received error for unknown endpoint: {}", status);
    return;
  }

  disconnect_from_scheduler(true);
  INFO("Disconnected from scheduler: {}", status);
}

bool StSender::register_with_scheduler() {
  auto header = std::as_bytes(std::span(sender_id_));
  return ucx::util::send_active_message(
      scheduler_ep_, AM_SENDER_REGISTER, header, {},
      on_scheduler_register_complete, this, 0);
}

void StSender::handle_scheduler_register_complete(ucs_status_ptr_t request,
                                                  ucs_status_t status) {
  scheduler_connecting_ = false;

  if (status != UCS_OK) {
    ERROR("Failed to register with scheduler: {}", status);
  } else {
    scheduler_connected_ = true;
    INFO("Successfully registered with scheduler");
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
};

void StSender::disconnect_from_scheduler(bool force) {
  scheduler_connecting_ = false;
  scheduler_connected_ = false;

  if (scheduler_ep_ == nullptr) {
    return;
  }

  flush_announced();
  ucx::util::close_endpoint(worker_, scheduler_ep_, force);
  scheduler_ep_ = nullptr;
  DEBUG("Disconnected from scheduler");
}

// Scheduler message handling

void StSender::send_announcement_to_scheduler(TsID id) {
  auto it = announced_.find(id);
  if (it == announced_.end()) {
    return;
  }

  const auto& [descriptor_bytes, iov_vector] = it->second;
  assert(iov_vector.size() > 0);
  uint64_t desc_size = descriptor_bytes.size();
  uint64_t content_size = 0;
  for (std::size_t i = 1; i < iov_vector.size(); ++i) {
    content_size += iov_vector[i].length;
  }
  std::array<uint64_t, 3> hdr{id, desc_size, content_size};

  auto header = std::as_bytes(std::span(hdr));
  auto buffer = std::span(static_cast<const std::byte*>(iov_vector[0].buffer),
                          iov_vector[0].length);

  ucx::util::send_active_message(scheduler_ep_, AM_SENDER_ANNOUNCE_ST, header,
                                 buffer, ucx::util::on_generic_send_complete,
                                 this, UCP_AM_SEND_FLAG_COPY_HEADER);
}

void StSender::send_retraction_to_scheduler(TsID id) {
  std::array<uint64_t, 1> hdr{id};

  auto header = std::as_bytes(std::span(hdr));
  ucx::util::send_active_message(scheduler_ep_, AM_SENDER_RETRACT_ST, header,
                                 {}, ucx::util::on_generic_send_complete, this,
                                 UCP_AM_SEND_FLAG_COPY_HEADER);
}

ucs_status_t
StSender::handle_scheduler_release(const void* header,
                                   size_t header_length,
                                   void* /* data */,
                                   size_t length,
                                   const ucp_am_recv_param_t* param) {
  TRACE(
      "Received scheduler release ST with header length {} and data length {}",
      header_length, length);

  if (header_length != sizeof(uint64_t) || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid scheduler request received");
    return UCS_OK;
  }

  auto id = *static_cast<const uint64_t*>(header);
  auto it = announced_.find(id);
  if (it != announced_.end()) {
    DEBUG("Removing released SubTimeslice with ID: {}", id);
    announced_.erase(it);
    complete_subtimeslice(id);
  } else {
    WARN("Release for unknown SubTimeslice ID: {}", id);
  }
  return UCS_OK;
}

// Builder connection management

void StSender::handle_new_connection(ucp_conn_request_h conn_request) {
  DEBUG("New connection request received");

  auto client_address = ucx::util::get_client_address(conn_request);
  if (!client_address) {
    ERROR("Failed to retrieve client address from connection request");
    ucp_listener_reject(listener_, conn_request);
    return;
  }

  auto ep = ucx::util::accept(worker_, conn_request, on_endpoint_error, this);
  if (!ep) {
    ERROR("Failed to create endpoint for new connection");
    return;
  }

  connections_[*ep] = *client_address;
  DEBUG("Accepted connection from {}", *client_address);
}

void StSender::handle_endpoint_error(ucp_ep_h ep, ucs_status_t status) {
  ERROR("Error on UCX endpoint: {}", status);

  auto it = connections_.find(ep);
  if (it != connections_.end()) {
    INFO("Removing disconnected endpoint: {}", it->second);
    connections_.erase(it);
  } else {
    ERROR("Received error for unknown endpoint");
  }
}

// Builder message handling

ucs_status_t
StSender::handle_builder_request(const void* header,
                                 size_t header_length,
                                 void* /* data */,
                                 size_t length,
                                 const ucp_am_recv_param_t* param) {
  TRACE("Received builder request ST with header length {} and data length {}",
        header_length, length);

  if (header_length != sizeof(uint64_t) || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid builder request received");
    return UCS_OK;
  }

  auto id = *static_cast<const uint64_t*>(header);
  send_subtimeslice_to_builder(id, param->reply_ep);
  return UCS_OK;
}

void StSender::send_subtimeslice_to_builder(TsID id, ucp_ep_h ep) {
  auto it = announced_.find(id);
  if (it == announced_.end()) {
    WARN("SubTimeslice with ID {} not found", id);
    std::array<uint64_t, 3> hdr{id, 0, 0};
    auto header = std::as_bytes(std::span(hdr));
    ucx::util::send_active_message(ep, AM_SENDER_SEND_ST, header, {},
                                   ucx::util::on_generic_send_complete, this,
                                   UCP_AM_SEND_FLAG_COPY_HEADER);
    return;
  }

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
  const auto& [descriptor_bytes, iov_vector] = it->second;
  uint64_t desc_size = descriptor_bytes.size();
  uint64_t content_size = 0;
  for (std::size_t i = 1; i < iov_vector.size(); ++i) {
    content_size += iov_vector[i].length;
  }
  std::array<uint64_t, 3> hdr{id, desc_size, content_size};

  // Send the data
  ucs_status_ptr_t request =
      ucp_am_send_nbx(ep, AM_SENDER_SEND_ST, hdr.data(), sizeof(hdr),
                      iov_vector.data(), iov_vector.size(), &req_param);

  if (UCS_PTR_IS_ERR(request)) {
    ucs_status_t status = UCS_PTR_STATUS(request);
    ERROR("Failed to send active message: {}", status);
    // Ignore the interaction with the builder, keep the announced subtimeslice
    return;
  }

  if (request == nullptr) {
    // Operation has completed successfully in-place
    TRACE("Active message sent successfully");
    return;
  }

  // Keep the element in announced_ until the send completes and store the
  // request
  active_send_requests_[request] = id;
}

void StSender::handle_builder_send_complete(void* request,
                                            ucs_status_t status) {
  if (UCS_PTR_IS_ERR(request)) {
    ERROR("Send operation failed: {}", status);
  } else if (status != UCS_OK) {
    ERROR("Send operation completed with status: {}", status);
  } else {
    TRACE("Send operation completed successfully");
  }

  auto it = active_send_requests_.find(request);
  if (it == active_send_requests_.end()) {
    ERROR("Received completion for unknown send request");
  } else {
    active_send_requests_.erase(request);
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
}

// Queue processing

void StSender::notify_queue_update() const {
  uint64_t value = 1;
  ssize_t ret = write(queue_event_fd_, &value, sizeof(value));
  if (ret != sizeof(value)) {
    ERROR("Failed to write to queue_event_fd_: {}", strerror(errno));
  }
}

std::size_t StSender::process_queues() {
  std::deque<std::pair<TsID, SubTimesliceHandle>> announcements;
  std::deque<TsID> retractions;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (pending_announcements_.empty() && pending_retractions_.empty()) {
      return 0;
    }
    announcements.swap(pending_announcements_);
    retractions.swap(pending_retractions_);
  }

  if (!scheduler_connected_) {
    TRACE("Scheduler not registered, skipping announcements");
    for (const auto& [id, sth] : announcements) {
      complete_subtimeslice(id);
    }
    return announcements.size();
  }

  for (auto id : retractions) {
    process_retraction(id);
  }
  for (const auto& [id, sth] : announcements) {
    process_announcement(id, sth);
  }

  return announcements.size() + retractions.size();
}

void StSender::process_announcement(TsID id, const SubTimesliceHandle& sth) {
  // Create and serialize subtimeslice
  StDescriptor descriptor = create_subtimeslice_descriptor(sth);
  auto descriptor_bytes = to_bytes(descriptor);

  std::vector<ucp_dt_iov> iov_vector = create_iov_vector(sth, descriptor_bytes);

  // Store for future use (and retention during send)
  announced_[id] = {std::move(descriptor_bytes), std::move(iov_vector)};

  // Ensure that the first iov component points to the string data
  assert(announced_[id].second.front().buffer == announced_[id].first.data());

  // Send announcement to scheduler
  send_announcement_to_scheduler(id);
}

void StSender::process_retraction(TsID id) {
  auto it = announced_.find(id);
  if (it != announced_.end()) {
    DEBUG("Retracting SubTimeslice with ID: {}", id);
    announced_.erase(it);
    send_retraction_to_scheduler(id);
    complete_subtimeslice(id);
  } else {
    WARN("Attempted to retract unknown SubTimeslice ID: {}", id);
  }
}

void StSender::complete_subtimeslice(TsID id) {
  // In the future, this could check if there is an ongoning send operation
  // concerning this SubTimeslice (cf. active_send_requests_) and wait for it to
  // complete before marking the SubTimeslice as completed. This would avoid
  // sending inconsistent data in special cases.
  std::lock_guard<std::mutex> lock(completions_mutex_);
  completed_.push(id);
}

void StSender::flush_announced() {
  for (const auto& [id, st] : announced_) {
    DEBUG("Flushing announced SubTimeslice with ID: {}", id);
    complete_subtimeslice(id);
  }
  announced_.clear();
}

// Helper methods

// Create subtimeslice structure for transmission to scheduler and builders.
// It contains, for each component, the offset and size of the
// descriptors and contents data blocks. The offsets are relative to the start
// of the overall data block and assume that all blocks are contiguous in
// memory.
StDescriptor
StSender::create_subtimeslice_descriptor(const SubTimesliceHandle& sth) {
  StDescriptor d;
  d.start_time_ns = sth.start_time_ns;
  d.duration_ns = sth.duration_ns;
  d.flags = sth.flags;

  std::ptrdiff_t offset = 0;
  for (const auto& c : sth.components) {
    std::size_t descriptors_size = 0;
    // Simply add all sizes as the blocks will be contiguous in memory after
    // transferring to the builder
    for (auto descriptor : c.descriptors) {
      descriptors_size += descriptor.length;
    }
    const DataDescriptor descriptor = {offset, descriptors_size};
    offset += descriptors_size;

    std::size_t content_size = 0;
    for (auto content : c.contents) {
      // Simply add all sizes as the blocks will be contiguous in memory after
      // transferring to the builder
      content_size += content.length;
    }
    const DataDescriptor content = {offset, content_size};
    offset += content_size;

    d.components.push_back({descriptor, content, c.flags});
  }

  return d;
}

// Create a vector of ucp_dt_iov structures for use with UCX send operations.
// The first element in the vector is the serialized descriptor string, followed
// by the descriptors and contents of each component in the shared memory.
std::vector<ucp_dt_iov>
StSender::create_iov_vector(const SubTimesliceHandle& sth,
                            std::span<std::byte> descriptor_bytes) {
  std::vector<ucp_dt_iov> iov_vector;

  // Add serialized descriptor at the beginning
  iov_vector.push_back({descriptor_bytes.data(), descriptor_bytes.size()});

  // Add component data from shared memory
  for (const auto& c : sth.components) {
    for (const auto& iov : c.descriptors) {
      iov_vector.push_back(iov);
    }
    for (const auto& iov : c.contents) {
      iov_vector.push_back(iov);
    }
  }

  return iov_vector;
}
