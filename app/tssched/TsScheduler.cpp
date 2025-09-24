/* Copyright (C) 2025 FIAS, Goethe-Universität Frankfurt am Main
   SPDX-License-Identifier: GPL-3.0-only
   Author: Jan de Cuveland */

#include "TsScheduler.hpp"
#include "SubTimeslice.hpp"
#include "System.hpp"
#include "TsbProtocol.hpp"
#include "Utility.hpp"
#include "log.hpp"
#include "ucxutil.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <sched.h>
#include <span>
#include <sys/socket.h>
#include <sys/types.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_compat.h>
#include <ucs/type/status.h>

TsScheduler::TsScheduler(volatile sig_atomic_t* signal_status,
                         uint16_t listen_port,
                         int64_t timeslice_duration_ns,
                         int64_t timeout_ns,
                         cbm::Monitor* monitor)
    : m_signal_status(signal_status), m_listen_port(listen_port),
      m_timeslice_duration_ns{timeslice_duration_ns}, m_timeout_ns{timeout_ns},
      m_hostname(fles::system::current_hostname()), m_monitor(monitor) {
  // Initialize event handling
  m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (m_epoll_fd == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }
}

TsScheduler::~TsScheduler() {
  if (m_epoll_fd != -1) {
    close(m_epoll_fd);
  }
}

// Main operation loop

void TsScheduler::run() {
  if (!ucx::util::init(m_context, m_worker, m_epoll_fd)) {
    ERROR("Failed to initialize UCX");
    return;
  }
  if (!ucx::util::set_receive_handler(m_worker, AM_SENDER_REGISTER,
                                      on_sender_register, this) ||
      !ucx::util::set_receive_handler(m_worker, AM_SENDER_ANNOUNCE_ST,
                                      on_sender_announce, this) ||
      !ucx::util::set_receive_handler(m_worker, AM_SENDER_RETRACT_ST,
                                      on_sender_retract, this) ||
      !ucx::util::set_receive_handler(m_worker, AM_BUILDER_REGISTER,
                                      on_builder_register, this) ||
      !ucx::util::set_receive_handler(m_worker, AM_BUILDER_STATUS,
                                      on_builder_status, this)) {
    ERROR("Failed to register receive handlers");
    return;
  }
  if (!ucx::util::create_listener(m_worker, m_listener, m_listen_port,
                                  on_new_connection, this)) {
    ERROR("Failed to create UCX listener at port {}", m_listen_port);
    return;
  }

  m_id = fles::system::current_time_ns() / m_timeslice_duration_ns;
  report_status();

  while (*m_signal_status == 0) {
    if (ucp_worker_progress(m_worker) != 0) {
      continue;
    }
    m_tasks.timer();

    bool try_later =
        m_senders.empty() ||
        std::any_of(m_senders.begin(), m_senders.end(), [this](const auto& s) {
          return s.second.last_received_st < m_id;
        });
    uint64_t current_time_ns = fles::system::current_time_ns();
    uint64_t timeout_ns = (m_id + 1) * m_timeslice_duration_ns + m_timeout_ns;

    if (try_later && current_time_ns < timeout_ns) {
      int sender_wait_ms =
          static_cast<int>((timeout_ns - current_time_ns + 999999) / 1000000);
      int timer_wait_ms = std::max(
          static_cast<int>(
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  m_tasks.when_next() - std::chrono::system_clock::now())
                  .count() +
              1),
          0);
      int timeout_ms = std::min(sender_wait_ms, timer_wait_ms);
      if (!ucx::util::arm_worker_and_wait(m_worker, m_epoll_fd, timeout_ms)) {
        break;
      }
      continue;
    }

    assign_timeslice(m_id);
    m_id++;
  }

  if (m_listener != nullptr) {
    ucp_listener_destroy(m_listener);
    m_listener = nullptr;
  }
  disconnect_from_all();
  ucx::util::cleanup(m_context, m_worker);
}

// Connection management

void TsScheduler::handle_new_connection(ucp_conn_request_h conn_request) {
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

  m_connections[*ep] = *client_address;
  DEBUG("Accepted connection from {}", *client_address);
}

void TsScheduler::handle_endpoint_error(ucp_ep_h ep, ucs_status_t status) {
  auto it = m_connections.find(ep);
  if (it != m_connections.end()) {
    m_connections.erase(it);
  } else {
    ERROR("Received error for unknown endpoint: {}", status);
  }

  auto sender_it = m_senders.find(ep);
  if (sender_it != m_senders.end()) {
    INFO("Disconnect from sender '{}': {}", sender_it->second.info.id(),
         status);
    m_senders.erase(sender_it);
  }

  if (auto it =
          std::find_if(m_builders.begin(), m_builders.end(),
                       [ep](const BuilderConnection& b) { return b.ep == ep; });
      it != m_builders.end()) {
    INFO("Disconnect from builder '{}': {}", it->info.id(), status);
    m_builders.erase(it);
  }

  // Fail any in-progress timeslice allocations that involved this endpoint
  // (TODO: not implemented yet)
}

void TsScheduler::disconnect_from_all() {
  if (m_connections.empty() && m_senders.empty() && m_builders.empty()) {
    return;
  }
  INFO("Disconnecting from {} senders and {} builders", m_senders.size(),
       m_builders.size());
  for (auto& [ep, _] : m_connections) {
    ucx::util::close_endpoint(m_worker, ep, true);
  }

  m_connections.clear();
  m_senders.clear();
  m_builders.clear();
}

// Sender message handling

ucs_status_t
TsScheduler::handle_sender_register(const void* header,
                                    size_t header_length,
                                    [[maybe_unused]] void* data,
                                    size_t length,
                                    const ucp_am_recv_param_t* param) {
  if (header_length == 0 || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid sender registration request received");
    return UCS_OK;
  }

  auto sender_info_bytes =
      std::span(static_cast<const std::byte*>(header), header_length);
  auto sender_info = to_obj_nothrow<SenderInfo>(sender_info_bytes);
  if (!sender_info) {
    ERROR("Failed to deserialize sender registration info");
    return UCS_OK;
  }

  ucp_ep_h ep = param->reply_ep;
  m_senders[ep] = {*sender_info, ep, {}};
  INFO("Accepted sender registration from '{}'", sender_info->id());

  return UCS_OK;
}

ucs_status_t
TsScheduler::handle_sender_announce(const void* header,
                                    size_t header_length,
                                    void* data,
                                    size_t length,
                                    const ucp_am_recv_param_t* param) {
  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 2 || length == 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid subtimeslice announcement received");
    return UCS_OK;
  }

  const TsId id = hdr[0];
  const uint64_t ms_data_size = hdr[1];

  auto it = m_senders.find(param->reply_ep);
  if (it == m_senders.end()) {
    ERROR("{}| Received announcement from unknown sender", id);
    return UCS_OK;
  }
  auto& sender_conn = it->second;

  auto st_descriptor_bytes =
      std::span(static_cast<const std::byte*>(data), length);
  auto st_descriptor = to_obj_nothrow<StDescriptor>(st_descriptor_bytes);
  if (!st_descriptor) {
    ERROR("{}| Failed to deserialize announcement from sender '{}'", id,
          sender_conn.info.id());
    return UCS_OK;
  }

  if (st_descriptor->duration_ns !=
      static_cast<uint64_t>(m_timeslice_duration_ns)) {
    ERROR("{}| Invalid timeslice duration from sender '{}'", id,
          sender_conn.info.id());
    return UCS_OK;
  }

  if (id < m_id) {
    DEBUG("{}| Late announcement from '{}', sending release", id,
          sender_conn.info.id());
    std::array<uint64_t, 1> hdr{id};
    auto header = std::as_bytes(std::span(hdr));
    ucx::util::send_active_message(param->reply_ep, AM_SCHED_RELEASE_ST, header,
                                   {}, ucx::util::on_generic_send_complete,
                                   this, UCP_AM_SEND_FLAG_COPY_HEADER);
    return UCS_OK;
  }

  sender_conn.announced_st.emplace_back(id, ms_data_size,
                                        std::move(*st_descriptor));
  sender_conn.last_received_st = id;
  DEBUG("{}| Announcement from '{}' ({})", id, sender_conn.info.id(),
        human_readable_count(ms_data_size, true));

  return UCS_OK;
}

ucs_status_t
TsScheduler::handle_sender_retract(const void* header,
                                   size_t header_length,
                                   [[maybe_unused]] void* data,
                                   size_t length,
                                   const ucp_am_recv_param_t* param) {
  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 1 || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid subtimeslice retraction received");
    return UCS_OK;
  }

  const TsId id = hdr[0];

  auto it = m_senders.find(param->reply_ep);
  if (it == m_senders.end()) {
    ERROR("Received retraction from unknown sender");
    return UCS_OK;
  }
  auto& conn = it->second;

  std::erase_if(conn.announced_st,
                [id](const auto& st) { return st.id == id; });

  return UCS_OK;
}

void TsScheduler::send_release_to_senders(TsId id) {
  std::array<uint64_t, 1> hdr{id};
  auto header = std::as_bytes(std::span(hdr));

  for (auto& [ep, conn] : m_senders) {
    ucx::util::send_active_message(ep, AM_SCHED_RELEASE_ST, header, {},
                                   ucx::util::on_generic_send_complete, this,
                                   UCP_AM_SEND_FLAG_COPY_HEADER);
    std::erase_if(conn.announced_st,
                  [id](const auto& st) { return st.id == id; });
  }
}

// Builder message handling
ucs_status_t
TsScheduler::handle_builder_register(const void* header,
                                     size_t header_length,
                                     [[maybe_unused]] void* data,
                                     size_t length,
                                     const ucp_am_recv_param_t* param) {
  if (header_length == 0 || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid builder registration request received");
    return UCS_OK;
  }

  auto builder_info_bytes =
      std::span(static_cast<const std::byte*>(header), header_length);
  auto builder_info = to_obj_nothrow<BuilderInfo>(builder_info_bytes);
  if (!builder_info) {
    ERROR("Failed to deserialize builder registration info");
    return UCS_OK;
  }

  ucp_ep_h ep = param->reply_ep;
  m_builders.emplace_back(*builder_info, ep, 0, false);
  INFO("Accepted builder registration from '{}'", builder_info->id());

  return UCS_OK;
}

ucs_status_t
TsScheduler::handle_builder_status(const void* header,
                                   size_t header_length,
                                   [[maybe_unused]] void* data,
                                   size_t length,
                                   const ucp_am_recv_param_t* param) {
  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 3 || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    ERROR("Invalid builder status received");
    return UCS_OK;
  }

  // Find the builder connection
  auto it = std::find_if(m_builders.begin(), m_builders.end(),
                         [ep = param->reply_ep](const BuilderConnection& b) {
                           return b.ep == ep;
                         });
  if (it == m_builders.end()) {
    ERROR("Received status from unknown builder");
    return UCS_OK;
  }
  const uint64_t event = hdr[0];
  const TsId id = hdr[1];
  const uint64_t new_bytes_free = hdr[2];

  switch (event) {
  case BUILDER_EVENT_NO_OP:
    if (new_bytes_free != it->bytes_available) {
      DEBUG("Builder '{}' reported free: {}", it->info.id(),
            human_readable_count(new_bytes_free, true));
    }
    if (new_bytes_free > it->bytes_available) {
      it->is_out_of_memory = false;
    }
    it->bytes_available = new_bytes_free;
    break;
  case BUILDER_EVENT_ALLOCATED:
    DEBUG("{}| Builder '{}' has allocated timeslice, now free: {}", id,
          it->info.id(), human_readable_count(new_bytes_free, true));
    it->bytes_available = new_bytes_free;
    break;
  case BUILDER_EVENT_OUT_OF_MEMORY:
    INFO("{}| Builder '{}' has reported out of memory", id, it->info.id());
    it->is_out_of_memory = true;
    assign_timeslice(id);
    break;
  case BUILDER_EVENT_RECEIVED:
    DEBUG("{}| Builder '{}' has received timeslice", id, it->info.id());
    send_release_to_senders(id);
    break;
  case BUILDER_EVENT_RELEASED:
    DEBUG("{}| Builder '{}' has released timeslice, now free: {}", id,
          it->info.id(), human_readable_count(new_bytes_free, true));
    if (new_bytes_free > it->bytes_available) {
      it->is_out_of_memory = false;
    }
    it->bytes_available = new_bytes_free;
    break;
  }

  return UCS_OK;
}

void TsScheduler::assign_timeslice(TsId id) {
  StCollection coll = create_collection_descriptor(id);
  if (coll.sender_ids.empty()) {
    if (!m_senders.empty()) {
      WARN("{}| Sender(s) connected ({}), but no contributions", id,
           m_senders.size());
    }
    return;
  }

  for (std::size_t i = 0; i < m_builders.size(); ++i) {
    auto& builder = m_builders[(id + i) % m_builders.size()];
    if (builder.bytes_available >= coll.ms_data_size() &&
        !builder.is_out_of_memory) {
      send_assignment_to_builder(coll, builder);
      DEBUG("{}| Assigned to '{}' ({}s, {})", id, builder.info.id(),
            coll.sender_ids.size(),
            human_readable_count(coll.ms_data_size(), true));
      return;
    }
  }
  WARN("{}| No builder available ({}s, {})", id, coll.sender_ids.size(),
       human_readable_count(coll.ms_data_size(), true));
  send_release_to_senders(id);
}

void TsScheduler::send_assignment_to_builder(const StCollection& coll,
                                             BuilderConnection& builder) {
  std::array<uint64_t, 2> hdr{coll.id, coll.ms_data_size()};
  auto header = std::as_bytes(std::span(hdr));
  auto buffer = std::make_unique<std::vector<std::byte>>(to_bytes(coll));
  auto* raw_ptr = buffer.release();

  ucx::util::send_active_message(
      builder.ep, AM_SCHED_ASSIGN_TS, header, *raw_ptr,
      [](void* request, ucs_status_t status, void* user_data) {
        auto buffer = std::unique_ptr<std::vector<std::byte>>(
            static_cast<std::vector<std::byte>*>(user_data));
        ucx::util::on_generic_send_complete(request, status, user_data);
      },
      raw_ptr, UCP_AM_SEND_FLAG_COPY_HEADER);
}

// Helper methods

StCollection TsScheduler::create_collection_descriptor(TsId id) {
  StCollection coll{id, {}, {}};
  for (auto& [sender_ep, sender] : m_senders) {
    auto it =
        std::find_if(sender.announced_st.begin(), sender.announced_st.end(),
                     [id](const auto& st) { return st.id == id; });
    if (it != sender.announced_st.end()) {
      coll.sender_ids.push_back(sender.info.advertise_id());
      coll.ms_data_sizes.push_back(it->ms_data_size);
      sender.announced_st.erase(it);
    } else {
      DEBUG("{}| No contribution found from sender '{}'", id, sender.info.id());
    }
  }
  return coll;
}

void TsScheduler::report_status() {
  constexpr auto interval = std::chrono::seconds(1);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  if (m_monitor != nullptr) {
    m_monitor->QueueMetric("tssched_status", {{"host", m_hostname}},
                           {{"timeslice_count", 0}});
  }
  // TODO: Add real metrics

  m_tasks.add([this] { report_status(); }, now + interval);
}
