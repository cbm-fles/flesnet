// Copyright 2025 Jan de Cuveland

#include "TsBuilder.hpp"
#include "SubTimeslice.hpp"
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
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_compat.h>
#include <ucs/type/status.h>

TsBuilder::TsBuilder(TsBuffer& timeslice_buffer,
                     std::string_view scheduler_address,
                     int64_t timeout_ns,
                     cbm::Monitor* monitor)
    : m_timeslice_buffer(timeslice_buffer),
      m_scheduler_address(scheduler_address), m_timeout_ns(timeout_ns),
      m_hostname(fles::system::current_hostname()), m_monitor(monitor) {
  // Initialize event handling
  m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (m_epoll_fd == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }

  // Start the worker thread
  m_worker_thread = std::jthread([this](std::stop_token st) { (*this)(st); });
}

TsBuilder::~TsBuilder() {
  if (m_worker_thread.joinable()) {
    m_worker_thread.request_stop();
    m_worker_thread.join();
  }

  if (m_epoll_fd != -1) {
    close(m_epoll_fd);
  }
}

// Main operation loop

void TsBuilder::operator()(std::stop_token stop_token) {
  cbm::system::set_thread_name("TsBuilder");

  if (!ucx::util::init(m_context, m_worker, m_epoll_fd)) {
    ERROR("Failed to initialize UCX");
    return;
  }
  if (!ucx::util::set_receive_handler(m_worker, AM_SCHED_SEND_TS,
                                      on_scheduler_send_ts, this) ||
      !ucx::util::set_receive_handler(m_worker, AM_SENDER_SEND_ST,
                                      on_sender_data, this)) {
    ERROR("Failed to register receive handlers");
    return;
  }
  connect_to_scheduler_if_needed();
  send_periodic_status_to_scheduler();
  report_status();

  while (!stop_token.stop_requested()) {
    if (ucp_worker_progress(m_worker) != 0) {
      continue;
    }
    if (auto id = m_timeslice_buffer.try_receive_completion()) {
      process_completion(*id);
      continue;
    }
    m_tasks.timer();

    if (!ucx::util::arm_worker_and_wait(m_worker, m_epoll_fd, 100)) {
      break;
    }
  }

  disconnect_from_scheduler();
  disconnect_from_senders();
  ucx::util::cleanup(m_context, m_worker);
}

// Scheduler connection management

void TsBuilder::connect_to_scheduler_if_needed() {
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  if (!m_scheduler_connecting && !m_scheduler_connected &&
      !m_worker_thread.get_stop_token().stop_requested()) {
    connect_to_scheduler();
  }

  m_tasks.add([this] { connect_to_scheduler_if_needed(); },
              now + m_scheduler_retry_interval);
}

void TsBuilder::connect_to_scheduler() {
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

  auto header = std::as_bytes(std::span(m_hostname));
  bool send_am_ok = ucx::util::send_active_message(
      m_scheduler_ep, AM_BUILDER_REGISTER, header, {},
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

void TsBuilder::handle_scheduler_error(ucp_ep_h ep, ucs_status_t status) {
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

void TsBuilder::handle_scheduler_register_complete(ucs_status_ptr_t request,
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
    INFO("Successfully registered with scheduler");
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
};

void TsBuilder::disconnect_from_scheduler(bool force) {
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
}

// Scheduler message handling

void TsBuilder::send_status_to_scheduler(uint64_t event, TsId id) {
  uint64_t bytes_free = m_timeslice_buffer.get_free_memory();
  std::array<uint64_t, 3> hdr{event, id, bytes_free};
  auto header = std::as_bytes(std::span(hdr));

  ucx::util::send_active_message(m_scheduler_ep, AM_BUILDER_STATUS, header, {},
                                 ucx::util::on_generic_send_complete, this,
                                 UCP_AM_SEND_FLAG_COPY_HEADER |
                                     UCP_AM_SEND_FLAG_REPLY);
}

void TsBuilder::send_periodic_status_to_scheduler() {
  constexpr auto interval = std::chrono::seconds(1);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  if (m_scheduler_connected) {
    send_status_to_scheduler(BUILDER_EVENT_NO_OP, 0);
  }

  m_tasks.add([this] { send_periodic_status_to_scheduler(); }, now + interval);
}

ucs_status_t TsBuilder::handle_scheduler_send_ts(
    const void* header,
    size_t header_length,
    void* data,
    size_t length,
    [[maybe_unused]] const ucp_am_recv_param_t* param) {
  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 2 || length == 0) {
    ERROR("Received invalid subtimeslice collection from scheduler");
    return UCS_OK;
  }

  const TsId id = hdr[0];
  const uint64_t ms_data_size = hdr[1];

  if (m_ts_handles.contains(id)) {
    ERROR("Received duplicate subtimeslice collection for {}", id);
    return UCS_OK;
  }

  auto desc = to_obj_nothrow<StCollection>(
      std::span(static_cast<const std::byte*>(data), length));
  if (!desc) {
    ERROR("Failed to deserialize subtimeslice collection for {}", id);
    return UCS_OK;
  }

  // Try to allocate memory for the content
  auto* buffer = m_timeslice_buffer.allocate(ms_data_size);
  if (buffer == nullptr) {
    INFO("Failed to allocate memory for timeslice {}", id);
    send_status_to_scheduler(BUILDER_EVENT_OUT_OF_MEMORY, id);
    return UCS_OK;
  }

  m_ts_handles.emplace(id,
                       std::make_unique<TsHandle>(buffer, std::move(*desc)));
  auto& tsh = *m_ts_handles.at(id);
  m_timeslice_count++;
  send_status_to_scheduler(BUILDER_EVENT_ALLOCATED, id);

  DEBUG("Received subtimeslice collection for {}, ms_data_size: {}", id,
        ms_data_size);

  // Ask senders for the contributions
  for (std::size_t i = 0; i < tsh.sender_ids.size(); ++i) {
    send_request_to_sender(tsh.sender_ids[i], id);
    update_st_state(tsh, i, StState::Requested);
  }

  // Handle timeout
  m_tasks.add(
      [&] {
        for (std::size_t i = 0; i < tsh.states.size(); ++i) {
          if (tsh.states[i] == StState::Requested ||
              tsh.states[i] == StState::Receiving) {
            update_st_state(tsh, i, StState::Failed);
          }
        }
      },
      std::chrono::system_clock::now() +
          std::chrono::nanoseconds(m_timeout_ns));

  return UCS_OK;
}

// Sender connection management

void TsBuilder::connect_to_sender(const std::string& sender_id) {
  auto [address, port] =
      ucx::util::parse_address(sender_id, DEFAULT_SENDER_PORT);
  auto ep =
      ucx::util::connect(m_worker, address, port, on_scheduler_error, this);
  if (ep) {
    DEBUG("Connecting to sender at '{}:{}'", address, port);
  } else {
    ERROR("Failed to connect to sender at '{}:{}'", address, port);
    return;
  }

  m_sender_to_ep[sender_id] = *ep;
  m_ep_to_sender[*ep] = sender_id;
}

void TsBuilder::handle_sender_error(ucp_ep_h ep, ucs_status_t status) {
  if (!m_ep_to_sender.contains(ep)) {
    ERROR("Received error for unknown sender endpoint: {}", status);
    return;
  }
  ucx::util::close_endpoint(m_worker, ep, true);

  auto sender = m_ep_to_sender[ep];
  INFO("Sender '{}' disconnected: {}", sender, status);

  m_ep_to_sender.erase(ep);
  m_sender_to_ep.erase(sender);
}

void TsBuilder::disconnect_from_senders() {
  if (m_ep_to_sender.empty()) {
    return;
  }
  INFO("Disconnecting from {} senders", m_ep_to_sender.size());
  for (const auto& [ep, _] : m_ep_to_sender) {
    ucx::util::close_endpoint(m_worker, ep, true);
  }

  m_ep_to_sender.clear();
  m_sender_to_ep.clear();
}

// Sender message handling

void TsBuilder::send_request_to_sender(const std::string& sender_id, TsId id) {
  if (!m_sender_to_ep.contains(sender_id)) {
    DEBUG("Connecting to sender '{}'", sender_id);
    connect_to_sender(sender_id);
    if (!m_sender_to_ep.contains(sender_id)) {
      return;
    }
  }

  auto* ep = m_sender_to_ep[sender_id];
  std::array<uint64_t, 1> hdr{id};
  auto header = std::as_bytes(std::span(hdr));

  ucx::util::send_active_message(ep, AM_BUILDER_REQUEST_ST, header, {},
                                 ucx::util::on_generic_send_complete, this,
                                 UCP_AM_SEND_FLAG_COPY_HEADER |
                                     UCP_AM_SEND_FLAG_REPLY);
}

ucs_status_t TsBuilder::handle_sender_data(const void* header,
                                           size_t header_length,
                                           void* data,
                                           size_t length,
                                           const ucp_am_recv_param_t* param) {
  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 3 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid subtimeslice data received>");
    DEBUG("hdr.size() = {}, length = {}, param->recv_attr = {}", hdr.size(),
          length, param->recv_attr);
    return UCS_OK;
  }

  const TsId id = hdr[0];
  const uint64_t st_descriptor_size = hdr[1];
  const uint64_t ms_data_size = hdr[2];

  if (st_descriptor_size + ms_data_size != length) {
    ERROR("Invalid header data in subtimeslice data received");
    return UCS_OK;
  }

  // Check if we have a sender connection for this endpoint
  ucp_ep_h ep = param->reply_ep;
  if (!m_ep_to_sender.contains(ep)) {
    ERROR("Received subtimeslice data from unknown sender endpoint");
    return UCS_OK;
  }
  const std::string& sender_id = m_ep_to_sender.at(ep);

  // Check if we have a handler for this TS id
  if (!m_ts_handles.contains(id)) {
    ERROR("Received subtimeslice data for unknown timeslice {}", id);
    return UCS_OK;
  }
  auto& tsh = *m_ts_handles.at(id);

  // Check if we have a contribution for this sender
  auto sender_id_it =
      std::find(tsh.sender_ids.begin(), tsh.sender_ids.end(), sender_id);
  if (sender_id_it == tsh.sender_ids.end()) {
    ERROR("Received subtimeslice data from unknown sender '{}' for {}",
          sender_id, id);
    return UCS_OK;
  }
  std::size_t ci = std::distance(tsh.sender_ids.begin(), sender_id_it);

  if (ms_data_size != tsh.ms_data_sizes[ci]) {
    ERROR("Unexpected ms_data_size from sender '{}' for {}, expected: "
          "{}, received: {}",
          sender_id, id, tsh.ms_data_sizes[ci], ms_data_size);
    update_st_state(tsh, ci, StState::Failed);
    return UCS_OK;
  }

  if ((param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) == 0) {
    ERROR("Received non-RNDV subtimeslice data from sender '{}' for {}",
          sender_id, id);
    update_st_state(tsh, ci, StState::Failed);
    return UCS_OK;
  }

  // RNDV receive: First desc_size bytes are the serialized StDescriptor,
  // remaining bytes are the content
  tsh.serialized_descriptors[ci].resize(st_descriptor_size);
  tsh.iovectors[ci][0] = {tsh.serialized_descriptors[ci].data(),
                          st_descriptor_size};
  tsh.iovectors[ci][1] = {tsh.buffer + tsh.offsets[ci], tsh.ms_data_sizes[ci]};

  ucp_request_param_t req_param{};
  req_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                           UCP_OP_ATTR_FIELD_USER_DATA |
                           UCP_OP_ATTR_FIELD_DATATYPE;
  req_param.cb.recv_am = on_sender_data_recv_complete;
  req_param.user_data = this;
  req_param.datatype = ucp_dt_make_iov();

  update_st_state(tsh, ci, StState::Receiving);
  ucs_status_ptr_t request =
      ucp_am_recv_data_nbx(m_worker, data, tsh.iovectors[ci].data(),
                           tsh.iovectors[ci].size(), &req_param);

  if (UCS_PTR_IS_ERR(request)) {
    ucs_status_t status = UCS_PTR_STATUS(request);
    ERROR("Failed to receive subtimeslice data from sender '{}' for {}: {}",
          sender_id, id, status);
    update_st_state(tsh, ci, StState::Failed);
    return UCS_OK;
  }

  if (request == nullptr) {
    // Operation has completed successfully in-place
    update_st_state(tsh, ci, StState::Complete);
    TRACE("Received subtimeslice data from sender '{}' for {}, "
          "st_descriptor_size: "
          "{}, ms_data_size: {}",
          sender_id, id, st_descriptor_size, ms_data_size);
    return UCS_OK;
  }

  m_active_data_recv_requests[request] = {id, ci};
  TRACE("Started receiving subtimeslice data from sender '{}'", sender_id);

  return UCS_OK;
}

void TsBuilder::handle_sender_data_recv_complete(
    void* request, ucs_status_t status, [[maybe_unused]] size_t length) {
  if (UCS_PTR_IS_ERR(request)) {
    ERROR("Data recv operation failed: {}", status);
  } else if (status != UCS_OK) {
    ERROR("Data recv operation completed with status: {}", status);
  } else {
    TRACE("Data recv operation completed successfully");
  }

  if (!m_active_data_recv_requests.contains(request)) {
    ERROR("Received completion for unknown data recv request");
  } else {
    auto [id, ci] = m_active_data_recv_requests.at(request);

    if (!m_ts_handles.contains(id)) {
      ERROR("Received completion for unknown TS {}", id);
      return;
    }
    auto& tsh = *m_ts_handles.at(id);
    if (ci >= tsh.ms_data_sizes.size()) {
      ERROR("Received completion for unknown contribution index {} for "
            "timeslice {}",
            ci, id);
      return;
    }
    m_component_count++;
    m_byte_count += tsh.ms_data_sizes[ci];
    update_st_state(tsh, ci, StState::Complete);
    m_active_data_recv_requests.erase(request);
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
}

// Queue processing

void TsBuilder::process_completion(TsId id) {
  if (!m_ts_handles.contains(id)) {
    ERROR("Received completion for unknown timeslice {}", id);
    return;
  }
  m_timeslice_buffer.deallocate(m_ts_handles.at(id)->buffer);
  m_ts_handles.erase(id);
  send_status_to_scheduler(BUILDER_EVENT_RELEASED, id);
  DEBUG("Processed timeslice completion for {}", id);
}

// Helper methods

void TsBuilder::update_st_state(TsHandle& tsh,
                                std::size_t contribution_index,
                                StState new_state) {
  assert(contribution_index < states.size());
  if (new_state == tsh.states[contribution_index]) {
    return;
  }
  tsh.states[contribution_index] = new_state;
  if (new_state == StState::Complete) {
    // Deserialize the descriptor
    auto desc = to_obj_nothrow<StDescriptor>(
        std::span(tsh.serialized_descriptors[contribution_index]));
    if (!desc) {
      ERROR("Failed to deserialize subtimeslice descriptor for TS {}", tsh.id);
      update_st_state(tsh, contribution_index, StState::Failed);
      return;
    }
    tsh.descriptors[contribution_index] = std::move(*desc);
  }
  if (new_state == StState::Complete || new_state == StState::Failed) {
    if (std::all_of(tsh.states.begin(), tsh.states.end(), [](StState state) {
          return state == StState::Complete || state == StState::Failed;
        })) {
      // All contributions are complete (or failed), publish the timeslice
      if (!tsh.is_published) {
        send_status_to_scheduler(BUILDER_EVENT_RECEIVED, tsh.id);
        StDescriptor ts_desc = create_timeslice_descriptor(tsh);
        m_timeslice_buffer.send_work_item(tsh.buffer, tsh.id, ts_desc);
        tsh.is_published = true;
        tsh.published_at_ns = fles::system::current_time_ns();
        if (ts_desc.has_flag(TsFlag::MissingSubtimeslices)) {
          INFO("Published incomplete timeslice {}", tsh.id);
          m_timeslice_incomplete_count++;
        } else {
          DEBUG("Published complete timeslice {}", tsh.id);
        }
      }
    }
  }
}

StDescriptor TsBuilder::create_timeslice_descriptor(TsHandle& tsh) {
  StDescriptor d{};
  for (std::size_t i = 0; i < tsh.sender_ids.size(); ++i) {
    if (tsh.states[i] != StState::Complete) {
      d.set_flag(TsFlag::MissingSubtimeslices);
      continue;
    }
    const auto& contrib = tsh.descriptors[i];
    if (d.duration_ns == 0) {
      d.start_time_ns = contrib.start_time_ns;
      d.duration_ns = contrib.duration_ns;
    } else if (d.start_time_ns != contrib.start_time_ns ||
               d.duration_ns != contrib.duration_ns) {
      ERROR("Inconsistent start time or duration in contributions");
      d.set_flag(TsFlag::MissingSubtimeslices);
      continue;
    }
    d.flags |= contrib.flags;
    for (const auto& c : contrib.components) {
      d.components.push_back(c);
      d.components.back().ms_data.offset += tsh.offsets[i];
    }
  }
  return d;
}

void TsBuilder::report_status() {
  constexpr auto interval = std::chrono::seconds(1);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  if (m_monitor != nullptr) {
    size_t timeslices_allocated = m_ts_handles.size();
    size_t bytes_allocated =
        m_timeslice_buffer.get_size() - m_timeslice_buffer.get_free_memory();
    m_monitor->QueueMetric(
        "tsbuild_status", {{"host", m_hostname}},
        {{"timeslice_count", m_timeslice_count},
         {"component_count", m_component_count},
         {"byte_count", m_byte_count},
         {"timeslice_incomplete_count", m_timeslice_incomplete_count},
         {"timeslices_allocated", timeslices_allocated},
         {"bytes_allocated", bytes_allocated}});
  }

  m_tasks.add([this] { report_status(); }, now + interval);
}
