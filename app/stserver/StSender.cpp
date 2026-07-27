/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "StSender.hpp"
#include "MicrosliceDescriptor.hpp"
#include "SubTimeslice.hpp"
#include "TsbProtocol.hpp"
#include "Utility.hpp"
#include "log.hpp"
#include "monitoring/SystemInfo.hpp"
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

namespace {

// Split a component's scatter-gather list into the two transfer blocks
// (microslice descriptors, then content) and append them to `blocks`. The
// split position is num_microslices * sizeof(MicrosliceDescriptor), which the
// builder derives independently from the merged descriptor to pre-post the
// matching receives. Without aggregation the iov boundary coincides with the
// split (descriptors and content come from different ring buffers); with
// aggregation the single contiguous iov is split arithmetically.
void append_component_blocks(std::vector<std::vector<ucp_dt_iov>>& blocks,
                             const StComponentHandle& component) {
  blocks.emplace_back();
  blocks.emplace_back();
  auto& desc_block = blocks[blocks.size() - 2];
  auto& content_block = blocks[blocks.size() - 1];

  uint64_t desc_remaining =
      component.num_microslices * sizeof(fles::MicrosliceDescriptor);
  for (ucp_dt_iov iov : component.ms_data) {
    if (iov.length == 0) {
      continue;
    }
    if (desc_remaining > 0) {
      if (iov.length <= desc_remaining) {
        desc_block.push_back(iov);
        desc_remaining -= iov.length;
        continue;
      }
      desc_block.push_back({iov.buffer, desc_remaining});
      iov.buffer = static_cast<std::byte*>(iov.buffer) + desc_remaining;
      iov.length -= desc_remaining;
      desc_remaining = 0;
    }
    content_block.push_back(iov);
  }
}

} // namespace

StSender::StSender(std::string_view manager_address,
                   uint16_t listen_port,
                   SenderInfo sender_info)
    : m_manager_address(manager_address), m_listen_port(listen_port),
      m_sender_info(std::move(sender_info)),
      m_sender_info_bytes(to_bytes(m_sender_info)) {
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
}

StSender::~StSender() {
  stop();

  if (m_epoll_fd != -1) {
    close(m_epoll_fd);
  }
  if (m_queue_event_fd != -1) {
    close(m_queue_event_fd);
  }
}

// Public API methods

void StSender::set_memory_region(std::span<std::byte> region) {
  m_memory_region = region;
}

void StSender::start() {
  m_worker_thread = std::jthread([this](std::stop_token st) {
    (*this)(st);
    m_thread_stopped = true;
  });
}

void StSender::stop() {
  if (m_worker_thread.joinable()) {
    m_worker_thread.request_stop();
    m_worker_thread.join();
  }
}

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

  if (!ucx::util::init(m_context, m_worker, m_epoll_fd, m_ucx_loop_mode)) {
    ERROR("Failed to initialize UCX");
    return;
  }
  if (!m_memory_region.empty()) {
    if (auto memh = ucx::util::register_memory(m_context, m_memory_region)) {
      m_buffer_memh = *memh;
    }
  }
  if (!ucx::util::set_receive_handler(m_worker, AM_MANAGER_RELEASE_ST,
                                      on_manager_release, this) ||
      !ucx::util::set_receive_handler(m_worker, AM_BUILDER_REQUEST_ST,
                                      on_builder_request, this)) {
    ERROR("Failed to register receive handlers");
    return;
  }
  connect_to_manager_if_needed();
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

    if (!ucx::util::arm_worker_and_wait(m_worker, m_epoll_fd,
                                        ucx::util::EPOLL_TIMEOUT_MS,
                                        m_ucx_loop_mode)) {
      break;
    }
  }

  if (m_listener != nullptr) {
    ucp_listener_destroy(m_listener);
    m_listener = nullptr;
  }
  disconnect_from_manager();
  disconnect_from_builders();
  // Drain remaining UCX internal operations (e.g., rendezvous protocol
  // buffers) before destroying the worker
  while (ucp_worker_progress(m_worker) != 0) {
  }
  if (m_buffer_memh != nullptr) {
    ucx::util::unregister_memory(m_context, m_buffer_memh);
    m_buffer_memh = nullptr;
  }
  ucx::util::cleanup(m_context, m_worker);
  flush_announced();
}

// Manager connection management

void StSender::connect_to_manager_if_needed() {
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  if (!m_manager_connecting && !m_manager_connected &&
      !m_worker_thread.get_stop_token().stop_requested()) {
    connect_to_manager();
  }

  m_tasks.add([this] { connect_to_manager_if_needed(); },
              now + m_manager_retry_interval);
}

void StSender::connect_to_manager() {
  assert(!m_manager_connecting && !m_manager_connected);

  auto [address, port] =
      ucx::util::parse_address(m_manager_address, DEFAULT_MANAGER_PORT);
  auto ep_result =
      ucx::util::connect(m_worker, address, port, on_manager_error, this);
  if (ep_result) {
    if (!m_mute_manager_reconnect) {
      INFO("Trying to connect to manager at '{}:{}'", address, port);
    }
  } else {
    if (!m_mute_manager_reconnect) {
      ERROR("Failed to connect to manager at '{}:{}': {}", address, port,
            ep_result.error());
      INFO("Will retry connection to manager every {}s",
           std::chrono::duration_cast<std::chrono::seconds>(
               m_manager_retry_interval)
               .count());
      m_mute_manager_reconnect = true;
    }
    return;
  }

  m_manager_ep = *ep_result;

  auto header = std::as_bytes(std::span(m_sender_info_bytes));
  bool send_am_ok = ucx::util::send_active_message(
      m_manager_ep, AM_SENDER_REGISTER, header, {},
      on_manager_register_complete, this, UCP_AM_SEND_FLAG_REPLY);
  if (!send_am_ok) {
    if (!m_mute_manager_reconnect) {
      WARN("Failed to register with manager at '{}:{}'", address, port);
      INFO("Will retry connection to manager every {}s",
           std::chrono::duration_cast<std::chrono::seconds>(
               m_manager_retry_interval)
               .count());
      m_mute_manager_reconnect = true;
    }
    ucx::util::close_endpoint(m_worker, m_manager_ep, true);
    m_manager_ep = nullptr;
    return;
  }

  m_manager_connecting = true;
}

void StSender::handle_manager_error(ucp_ep_h ep, ucs_status_t status) {
  if (ep != m_manager_ep) {
    ERROR("Received error for unknown endpoint: {}", status);
    return;
  }

  if (m_manager_connected) {
    WARN("Disconnected from manager: {}", status);
  }
  m_manager_connected = false;
  disconnect_from_manager(true);
}

void StSender::handle_manager_register_complete(ucs_status_ptr_t request,
                                                ucs_status_t status) {
  m_manager_connecting = false;

  if (status != UCS_OK) {
    if (!m_mute_manager_reconnect) {
      WARN("Failed to register with manager: {}", status);
      INFO("Will retry connection to manager every {}s",
           std::chrono::duration_cast<std::chrono::seconds>(
               m_manager_retry_interval)
               .count());
      m_mute_manager_reconnect = true;
    }
  } else {
    m_manager_connected = true;
    m_mute_manager_reconnect = false;
    INFO("Registered with manager");
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
};

void StSender::disconnect_from_manager(bool force) {
  if (m_manager_connected) {
    INFO("Disconnecting from manager");
  }
  m_manager_connecting = false;
  m_manager_connected = false;

  if (m_manager_ep == nullptr) {
    return;
  }

  ucx::util::close_endpoint(m_worker, m_manager_ep, force);
  m_manager_ep = nullptr;

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

// Manager message handling

void StSender::do_announce_subtimeslice(TsId id, const StHandle& sth) {
  // Create subtimeslice structure for transmission to manager and builders.
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

  // Serialize subtimeslice structure for transmission to the manager. The
  // builder no longer receives this descriptor directly; the manager relays
  // the (merged) descriptor as part of the assignment.
  auto st_descriptor_bytes = serialize_descriptor(st_descriptor);

  // Assemble the scatter-gather lists of the transfer blocks (two per
  // component) pointing at the (registered) microslice data. Each block is
  // later sent as one tagged message into a receive the builder pre-posts
  // from the same layout information.
  std::vector<std::vector<ucp_dt_iov>> blocks;
  for (const auto& c : sth.components) {
    append_component_blocks(blocks, c);
  }
  if (blocks.empty()) {
    // No components: exchange a single zero-size message so the builder's
    // pre-posted receive completes and the protocol stays synchronous
    blocks.emplace_back();
  }

  // Store for future use (and retention during send)
  m_announced.emplace(
      id, std::make_unique<AnnouncementHandle>(
              id, std::move(st_descriptor_bytes), std::move(blocks)));
  auto& ah = *m_announced.at(id);

  DEBUG("{}| Announcing ({}c, {}m, {}, flags={:04x})", id,
        st_descriptor.components.size(), num_microslices,
        human_readable_count(ms_data_size, true), st_descriptor.flags);

  // Send announcement to manager
  std::array<uint64_t, 2> hdr{id, ms_data_size};
  auto header = std::as_bytes(std::span(hdr));
  auto buffer = std::as_bytes(std::span(ah.st_descriptor_bytes));
  ucx::util::send_active_message(
      m_manager_ep, AM_SENDER_ANNOUNCE_ST, header, buffer,
      ucx::util::on_generic_send_complete, this,
      UCP_AM_SEND_FLAG_COPY_HEADER | UCP_AM_SEND_FLAG_REPLY);
}

void StSender::do_retract_subtimeslice(TsId id) {
  auto it = m_announced.find(id);
  if (it != m_announced.end()) {
    auto& ah = *it->second;
    if (!ah.pending_release) {
      DEBUG("{}| Retracting subtimeslice", id);

      // Send retraction to manager
      std::array<uint64_t, 1> hdr{id};
      auto header = std::as_bytes(std::span(hdr));
      ucx::util::send_active_message(
          m_manager_ep, AM_SENDER_RETRACT_ST, header, {},
          ucx::util::on_generic_send_complete, this,
          UCP_AM_SEND_FLAG_COPY_HEADER | UCP_AM_SEND_FLAG_REPLY);

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
      WARN("{}| Attempted to retract subtimeslice already marked for release",
           id);
    }
  } else {
    WARN("{}| Attempted to retract unknown subtimeslice", id);
  }
}

ucs_status_t StSender::handle_manager_release(
    const void* header,
    size_t header_length,
    [[maybe_unused]] void* data,
    size_t length,
    [[maybe_unused]] const ucp_am_recv_param_t* param) {
  if (header_length != sizeof(uint64_t) || length != 0) {
    ERROR("Invalid manager request received");
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
  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 2 || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid builder request received");
    return UCS_OK;
  }

  TsId id = hdr[0];
  uint64_t tag = hdr[1];
  send_subtimeslice_to_builder(id, param->reply_ep, tag);
  return UCS_OK;
}

void StSender::send_subtimeslice_to_builder(TsId id,
                                            ucp_ep_h ep,
                                            uint64_t tag) {
  if (!m_announced.contains(id)) {
    // Subtimeslice not found: send a zero-byte tag-matched message so the
    // builder's first pre-posted recv completes (with a length of 0 it will
    // be marked as Failed there and the remaining pre-posted recvs of this
    // contribution are canceled). This keeps the protocol synchronous.
    WARN("{}| Subtimeslice not found", id);
    ucp_request_param_t req_param{};
    req_param.op_attr_mask =
        UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
    req_param.cb.send = ucx::util::on_generic_send_complete;
    req_param.user_data = this;
    ucs_status_ptr_t request =
        ucp_tag_send_nbx(ep, nullptr, 0, tag, &req_param);
    if (UCS_PTR_IS_ERR(request)) {
      ERROR("Failed to send empty tag send: {}", UCS_PTR_STATUS(request));
    }
    return;
  }
  auto& ah = *m_announced.at(id);

  // Send the transfer blocks as separate tagged messages, all with the same
  // tag: tag matching is FIFO per tag, so they complete the builder's
  // pre-posted receives in order. A block is sent with the default contiguous
  // datatype whenever it consists of a single fragment (always the case with
  // the aggregation buffer): for the iov datatype UCX cannot use the
  // single-RDMA-read rendezvous protocol and falls back to fragmented sends
  // at roughly half the achievable bandwidth. Only blocks split by a ring
  // buffer wrap-around still use the iov datatype.
  DEBUG("{}| Sending {} blocks to builder '{}'", id, ah.blocks.size(),
        m_builders[ep]);
  for (const auto& block : ah.blocks) {
    ucp_request_param_t req_param{};
    req_param.op_attr_mask =
        UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
    req_param.cb.send = on_builder_send_complete;
    req_param.user_data = this;

    const void* send_buffer = nullptr;
    size_t send_count = 0;
    if (block.size() == 1) {
      send_buffer = block[0].buffer;
      send_count = block[0].length;
    } else if (block.size() > 1) {
      req_param.op_attr_mask |= UCP_OP_ATTR_FIELD_DATATYPE;
      req_param.datatype = ucp_dt_make_iov();
      send_buffer = block.data();
      send_count = block.size();
    }

    ucs_status_ptr_t request =
        ucp_tag_send_nbx(ep, send_buffer, send_count, tag, &req_param);

    if (UCS_PTR_IS_ERR(request)) {
      ucs_status_t status = UCS_PTR_STATUS(request);
      ERROR("Failed to send tag message: {}", status);
      // Stop sending; the builder handles the missing blocks via its timeout.
      // Keep the announced subtimeslice.
      return;
    }

    if (request == nullptr) {
      // Operation has completed successfully in-place
      continue;
    }

    // Keep the element in m_announced until the send completes and store the
    // request
    m_active_send_requests[request] = id;
    ah.active_send_requests++;
  }
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

  // Collect endpoints and clear map before closing, so that error
  // callbacks during close do not modify the map during iteration
  std::vector<ucp_ep_h> eps_to_close;
  eps_to_close.reserve(m_builders.size());
  for (auto& [ep, _] : m_builders) {
    eps_to_close.push_back(ep);
  }
  m_builders.clear();

  for (auto* ep : eps_to_close) {
    ucx::util::close_endpoint(m_worker, ep, true);
  }
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

  if (!m_manager_connected) {
    // Manager not registered, skipping announcements
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
