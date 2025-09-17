// Copyright 2025 Jan de Cuveland

#include "StSender.hpp"
#include "SubTimeslice.hpp"
#include "System.hpp"
#include "TsbProtocol.hpp"
#include "Utility.hpp"
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
    : m_scheduler_address(scheduler_address), m_listen_port(listen_port) {
  m_sender_id =
      fles::system::current_hostname() + ":" + std::to_string(m_listen_port);

  // Initialize event handling
  m_queue_event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (m_queue_event_fd == -1) {
    throw std::runtime_error("eventfd failed");
  }
  m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (m_epoll_fd == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }

  // Add message queue's eventfd to epoll
  epoll_event ev{};
  ev.events = EPOLLIN | EPOLLET; // Edge-triggered
  ev.data.fd = m_queue_event_fd;
  if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_queue_event_fd, &ev) == -1) {
    throw std::runtime_error("epoll_ctl failed for message queue");
  }

  // Start the worker thread
  m_worker_thread = std::jthread([this](std::stop_token st) {
    (*this)(st);
    m_thread_stopped = true;
  });
}

StSender::~StSender() {
  if (m_worker_thread.joinable()) {
    m_worker_thread.request_stop();
    m_worker_thread.join();
  }

  if (m_epoll_fd != -1) {
    close(m_epoll_fd);
  }
  if (m_queue_event_fd != -1) {
    close(m_queue_event_fd);
  }
}

// Public API methods

void StSender::announce_subtimeslice(TsId id, const StHandle& st) {
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_pending_announcements.emplace_back(id, st);
  }
  notify_queue_update();
}

void StSender::retract_subtimeslice(TsId id) {
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    auto it = std::find_if(m_pending_announcements.begin(),
                           m_pending_announcements.end(),
                           [id](const auto& item) { return item.first == id; });
    if (it != m_pending_announcements.end()) {
      {
        std::lock_guard<std::mutex> lock(m_completions_mutex);
        m_completed.push(id);
      }
      m_pending_announcements.erase(it);
      return;
    }
    m_pending_retractions.emplace_back(id);
  }
  notify_queue_update();
}

std::optional<TsId> StSender::try_receive_completion() {
  std::lock_guard<std::mutex> lock(m_completions_mutex);
  if (m_completed.empty()) {
    return std::nullopt;
  }
  TsId id = m_completed.front();
  m_completed.pop();
  return id;
}

// Main operation loop

void StSender::operator()(std::stop_token stop_token) {
  cbm::system::set_thread_name("StSender");

  if (!ucx::util::init(m_context, m_worker, m_epoll_fd)) {
    ERROR("Failed to initialize UCX");
    return;
  }
  if (!ucx::util::set_receive_handler(m_worker, AM_SCHED_RELEASE_ST,
                                      on_scheduler_release, this) ||
      !ucx::util::set_receive_handler(m_worker, AM_BUILDER_REQUEST_ST,
                                      on_builder_request, this)) {
    ERROR("Failed to register receive handlers");
    return;
  }
  connect_to_scheduler_if_needed();
  if (!ucx::util::create_listener(m_worker, m_listener, m_listen_port,
                                  on_new_connection, this)) {
    ERROR("Failed to create UCX listener at port {}", m_listen_port);
    return;
  }

  while (!stop_token.stop_requested()) {
    if (ucp_worker_progress(m_worker) != 0) {
      continue;
    }
    if (process_queues() > 0) {
      continue;
    }
    m_tasks.timer();

    if (!ucx::util::arm_worker_and_wait(m_worker, m_epoll_fd)) {
      break;
    }
  }

  if (m_listener != nullptr) {
    ucp_listener_destroy(m_listener);
    m_listener = nullptr;
  }
  disconnect_from_scheduler();
  disconnect_from_builders();
  ucx::util::cleanup(m_context, m_worker);
}

// Scheduler connection management

void StSender::connect_to_scheduler_if_needed() {
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  if (!m_scheduler_connecting && !m_scheduler_connected &&
      !m_worker_thread.get_stop_token().stop_requested()) {
    connect_to_scheduler();
  }

  m_tasks.add([this] { connect_to_scheduler_if_needed(); },
              now + m_scheduler_retry_interval);
}

void StSender::connect_to_scheduler() {
  assert(!m_scheduler_connecting && !m_scheduler_connected);

  auto [address, port] =
      ucx::util::parse_address(m_scheduler_address, DEFAULT_SCHEDULER_PORT);
  auto ep =
      ucx::util::connect(m_worker, address, port, on_scheduler_error, this);
  if (ep) {
    if (!m_mute_scheduler_reconnect) {
      INFO("Trying to connect to scheduler at '{}:{}'", address, port);
    }
  } else {
    WARN("Failed to connect to scheduler at '{}:{}', will retry", address,
         port);
    return;
  }

  m_scheduler_ep = *ep;

  auto header = std::as_bytes(std::span(m_sender_id));
  bool send_am_ok = ucx::util::send_active_message(
      m_scheduler_ep, AM_SENDER_REGISTER, header, {},
      on_scheduler_register_complete, this, UCP_AM_SEND_FLAG_REPLY);
  if (!send_am_ok) {
    WARN("Failed to register with scheduler at '{}:{}', will retry", address,
         port);
    ucx::util::close_endpoint(m_worker, m_scheduler_ep, true);
    m_scheduler_ep = nullptr;
    return;
  }

  m_scheduler_connecting = true;
}

void StSender::handle_scheduler_error(ucp_ep_h ep, ucs_status_t status) {
  if (ep != m_scheduler_ep) {
    ERROR("Received error for unknown endpoint: {}", status);
    return;
  }

  if (m_scheduler_connected) {
    WARN("Disconnected from scheduler: {}", status);
  }
  m_scheduler_connected = false;
  disconnect_from_scheduler(true);
}

void StSender::handle_scheduler_register_complete(ucs_status_ptr_t request,
                                                  ucs_status_t status) {
  m_scheduler_connecting = false;

  if (status != UCS_OK) {
    if (!m_mute_scheduler_reconnect) {
      WARN("Failed to register with scheduler: {}", status);
      INFO("Will retry connection to scheduler every {}s",
           std::chrono::duration_cast<std::chrono::seconds>(
               m_scheduler_retry_interval)
               .count());
      m_mute_scheduler_reconnect = true;
    }
  } else {
    m_scheduler_connected = true;
    m_mute_scheduler_reconnect = false;
    INFO("Registered with scheduler");
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
};

void StSender::disconnect_from_scheduler(bool force) {
  if (m_scheduler_connected) {
    INFO("Disconnecting from scheduler");
  }
  m_scheduler_connecting = false;
  m_scheduler_connected = false;

  if (m_scheduler_ep == nullptr) {
    return;
  }

  ucx::util::close_endpoint(m_worker, m_scheduler_ep, force);
  m_scheduler_ep = nullptr;

  // Flush all announced subtimeslices
  auto it = m_announced.begin();
  while (it != m_announced.end()) {
    const auto& [id, ah] = *it;
    if (ah->active_send_requests > 0) {
      DEBUG("{}| Marking for release (currently sending)", id);
      ah->pending_release = true;
      ++it;
    } else {
      DEBUG("{}| Releasing", id);
      {
        std::lock_guard<std::mutex> lock(m_completions_mutex);
        m_completed.push(id);
      }
      it = m_announced.erase(it);
    }
  }
}

// Scheduler message handling

void StSender::do_announce_subtimeslice(TsId id, const StHandle& sth) {
  // Create subtimeslice structure for transmission to scheduler and builders.
  // It contains, for each component, the offset and size of the microslice
  // descriptors and microslice contents data blocks. The offsets are relative
  // to the start of the overall data block and assume that all blocks are
  // contiguous in memory.
  StDescriptor st_descriptor;
  st_descriptor.start_time_ns = sth.start_time_ns;
  st_descriptor.duration_ns = sth.duration_ns;
  st_descriptor.flags = sth.flags;

  std::size_t ms_data_size = 0;
  std::size_t num_microslices = 0;
  for (const auto& c : sth.components) {
    // Simply add all sizes as the blocks will be contiguous in memory after
    // transferring to the builder
    const std::size_t component_size = c.ms_data_size();
    st_descriptor.components.push_back({static_cast<ptrdiff_t>(ms_data_size),
                                        component_size, c.num_microslices,
                                        c.flags});
    ms_data_size += component_size;
    num_microslices += c.num_microslices;
  }

  // Serialize subtimeslice structure
  auto st_descriptor_bytes = to_bytes(st_descriptor);

  // Assemble a vector of ucp_dt_iov structures for use with UCX send
  // operations. The first element in the vector is the serialized descriptor
  // string, followed by the descriptors and contents of each component in the
  // shared memory.
  std::vector<ucp_dt_iov> iov_vector;
  iov_vector.push_back(
      {st_descriptor_bytes.data(), st_descriptor_bytes.size()});
  for (const auto& c : sth.components) {
    iov_vector.insert(iov_vector.end(), c.ms_data.begin(), c.ms_data.end());
  }

  // Store for future use (and retention during send)
  m_announced.emplace(
      id, std::make_unique<AnnouncementHandle>(
              id, std::move(st_descriptor_bytes), std::move(iov_vector)));
  auto& ah = *m_announced.at(id);

  // Ensure that the first iov component points to the string data
  assert(ah.iovector.front().buffer == ah.st_descriptor_bytes.data());

  DEBUG("{}| Announcing ({}c, {}m, {}, flags={:04x})", id,
        st_descriptor.components.size(), num_microslices,
        human_readable_count(ms_data_size), st_descriptor.flags);

  // Send announcement to scheduler
  std::array<uint64_t, 2> hdr{id, ms_data_size};
  auto header = std::as_bytes(std::span(hdr));
  auto buffer = std::as_bytes(std::span(ah.st_descriptor_bytes));
  ucx::util::send_active_message(
      m_scheduler_ep, AM_SENDER_ANNOUNCE_ST, header, buffer,
      ucx::util::on_generic_send_complete, this,
      UCP_AM_SEND_FLAG_COPY_HEADER | UCP_AM_SEND_FLAG_REPLY);
}

void StSender::do_retract_subtimeslice(TsId id) {
  auto it = m_announced.find(id);
  if (it != m_announced.end()) {
    DEBUG("{}| Retracting subtimeslice", id);

    // Send retraction to scheduler
    std::array<uint64_t, 1> hdr{id};
    auto header = std::as_bytes(std::span(hdr));
    ucx::util::send_active_message(
        m_scheduler_ep, AM_SENDER_RETRACT_ST, header, {},
        ucx::util::on_generic_send_complete, this,
        UCP_AM_SEND_FLAG_COPY_HEADER | UCP_AM_SEND_FLAG_REPLY);

    auto& ah = *it->second;
    if (ah.active_send_requests > 0) {
      DEBUG("{}| Marking for release (currently sending)", id);
      ah.pending_release = true;
    } else {
      {
        std::lock_guard<std::mutex> lock(m_completions_mutex);
        m_completed.push(id);
      }
      m_announced.erase(it);
    }
  } else {
    WARN("{}| Attempted to retract unknown subtimeslice", id);
  }
}

ucs_status_t StSender::handle_scheduler_release(
    const void* header,
    size_t header_length,
    [[maybe_unused]] void* data,
    size_t length,
    [[maybe_unused]] const ucp_am_recv_param_t* param) {
  if (header_length != sizeof(uint64_t) || length != 0) {
    ERROR("Invalid scheduler request received");
    return UCS_OK;
  }

  TsId id = *static_cast<const uint64_t*>(header);
  auto it = m_announced.find(id);
  if (it != m_announced.end()) {
    auto& ah = *it->second;
    if (ah.active_send_requests > 0) {
      DEBUG("{}| Marking for release (currently sending)", id);
      ah.pending_release = true;
    } else {
      DEBUG("{}| Releasing", id);
      {
        std::lock_guard<std::mutex> lock(m_completions_mutex);
        m_completed.push(id);
      }
      m_announced.erase(it);
    }
  } else {
    WARN("{}| Received release for unknown subtimeslice", id);
  }
  return UCS_OK;
}

// Builder connection management

void StSender::handle_new_connection(ucp_conn_request_h conn_request) {
  DEBUG("New connection request received");

  auto client_address = ucx::util::get_client_address(conn_request);
  if (!client_address) {
    ERROR("Failed to retrieve client address from connection request");
    ucp_listener_reject(m_listener, conn_request);
    return;
  }

  auto ep = ucx::util::accept(m_worker, conn_request, on_endpoint_error, this);
  if (!ep) {
    ERROR("Failed to create endpoint for new connection");
    return;
  }

  m_builders[*ep] = *client_address;
  DEBUG("Accepted connection from '{}'", *client_address);
}

void StSender::handle_endpoint_error(ucp_ep_h ep, ucs_status_t status) {
  auto it = m_builders.find(ep);
  if (it != m_builders.end()) {
    INFO("Disconnect from builder '{}': {}", it->second, status);
    m_builders.erase(it);
  } else {
    ERROR("Received error for unknown endpoint: {}", status);
  }
}

// Builder message handling

ucs_status_t
StSender::handle_builder_request(const void* header,
                                 size_t header_length,
                                 [[maybe_unused]] void* data,
                                 size_t length,
                                 const ucp_am_recv_param_t* param) {
  if (header_length != sizeof(uint64_t) || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid builder request received");
    return UCS_OK;
  }

  auto id = *static_cast<const uint64_t*>(header);
  send_subtimeslice_to_builder(id, param->reply_ep);
  return UCS_OK;
}

void StSender::send_subtimeslice_to_builder(TsId id, ucp_ep_h ep) {
  if (!m_announced.contains(id)) {
    WARN("{}| Subtimeslice not found", id);
    std::array<uint64_t, 3> hdr{id, 0, 0};
    auto header = std::as_bytes(std::span(hdr));
    ucx::util::send_active_message(
        ep, AM_SENDER_SEND_ST, header, {}, ucx::util::on_generic_send_complete,
        this, UCP_AM_SEND_FLAG_COPY_HEADER | UCP_AM_SEND_FLAG_REPLY);
    return;
  }
  auto& ah = *m_announced.at(id);

  // Prepare send parameters
  ucp_request_param_t req_param{};
  req_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_FLAGS | UCP_OP_ATTR_FIELD_CALLBACK |
      UCP_OP_ATTR_FIELD_USER_DATA | UCP_OP_ATTR_FIELD_DATATYPE;
  req_param.flags = UCP_AM_SEND_FLAG_COPY_HEADER | UCP_AM_SEND_FLAG_REPLY |
                    UCP_AM_SEND_FLAG_RNDV;
  req_param.cb.send = on_builder_send_complete;
  req_param.user_data = this;
  req_param.datatype = ucp_dt_make_iov();

  // Prepare header data
  uint64_t st_descriptor_size = ah.st_descriptor_bytes.size();
  uint64_t ms_data_size = 0;
  for (std::size_t i = 1; i < ah.iov_vector.size(); ++i) {
    ms_data_size += ah.iov_vector[i].length;
  }
  std::array<uint64_t, 3> hdr{id, st_descriptor_size, ms_data_size};

  // Send the data
  DEBUG("{}| Sending to builder '{}'", id, m_builders[ep]);
  ucs_status_ptr_t request =
      ucp_am_send_nbx(ep, AM_SENDER_SEND_ST, hdr.data(), sizeof(hdr),
                      ah.iov_vector.data(), ah.iov_vector.size(), &req_param);

  if (UCS_PTR_IS_ERR(request)) {
    ucs_status_t status = UCS_PTR_STATUS(request);
    ERROR("Failed to send active message: {}", status);
    // Ignore the interaction with the builder, keep the announced subtimeslice
    return;
  }

  if (request == nullptr) {
    // Operation has completed successfully in-place
    return;
  }

  // Keep the element in m_announced until the send completes and store the
  // request
  m_active_send_requests[request] = id;
  ah.active_send_requests++;
}

void StSender::handle_builder_send_complete(void* request,
                                            ucs_status_t status) {
  if (UCS_PTR_IS_ERR(request)) {
    ERROR("Send operation failed: {}", status);
  } else if (status != UCS_OK) {
    ERROR("Send operation completed with status: {}", status);
  }

  if (!m_active_send_requests.contains(request)) {
    ERROR("Received completion for unknown send request");
  } else {
    TsId id = m_active_send_requests.at(request);
    if (!m_announced.contains(id)) {
      ERROR("{}| Sent subtimeslice not found in announced list", id);
    } else {
      auto& ah = *m_announced.at(id);
      ah.active_send_requests--;
      if (ah.pending_release && ah.active_send_requests == 0) {
        DEBUG("{}| Releasing after send completion", id);
        {
          std::lock_guard<std::mutex> lock(m_completions_mutex);
          m_completed.push(id);
        }
        m_announced.erase(id);
      }
    }
    m_active_send_requests.erase(request);
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
}

void StSender::disconnect_from_builders() {
  if (m_builders.empty()) {
    return;
  }
  INFO("Disconnecting from {} builders", m_builders.size());
  for (auto& [ep, _] : m_builders) {
    ucx::util::close_endpoint(m_worker, ep, true);
  }

  m_builders.clear();
}

// Queue processing

void StSender::notify_queue_update() const {
  uint64_t value = 1;
  ssize_t ret = write(m_queue_event_fd, &value, sizeof(value));
  if (ret != sizeof(value)) {
    ERROR("Failed to write to m_queue_event_fd: {}", strerror(errno));
  }
}

std::size_t StSender::process_queues() {
  std::deque<std::pair<TsId, StHandle>> announcements;
  std::deque<TsId> retractions;
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    if (m_pending_announcements.empty() && m_pending_retractions.empty()) {
      return 0;
    }
    announcements.swap(m_pending_announcements);
    retractions.swap(m_pending_retractions);
  }

  if (!m_scheduler_connected) {
    // Scheduler not registered, skipping announcements
    for (const auto& [id, sth] : announcements) {
      std::lock_guard<std::mutex> lock(m_completions_mutex);
      m_completed.push(id);
    }
    return announcements.size();
  }

  for (auto id : retractions) {
    do_retract_subtimeslice(id);
  }
  for (const auto& [id, sth] : announcements) {
    do_announce_subtimeslice(id, sth);
  }

  return announcements.size() + retractions.size();
}

void StSender::flush_announced() {
  for (const auto& [id, st] : m_announced) {
    DEBUG("{}| Flushing announced subtimeslice", id);
    std::lock_guard<std::mutex> lock(m_completions_mutex);
    m_completed.push(id);
  }
  m_announced.clear();
}
