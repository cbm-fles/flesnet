// Copyright 2025 Jan de Cuveland

#include "TsScheduler.hpp"
#include "SubTimeslice.hpp"
#include "System.hpp"
#include "TsbProtocol.hpp"
#include "log.hpp"
#include "monitoring/System.hpp"
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

TsScheduler::TsScheduler(uint16_t listen_port,
                         int64_t timeslice_duration_ns,
                         int64_t timeout_ns,
                         cbm::Monitor* monitor)
    : listen_port_(listen_port), timeslice_duration_ns_{timeslice_duration_ns},
      timeout_ns_{timeout_ns}, hostname_(fles::system::current_hostname()),
      monitor_(monitor) {
  // Initialize event handling
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }

  // Start the worker thread
  worker_thread_ = std::jthread([this](std::stop_token st) { (*this)(st); });
}

TsScheduler::~TsScheduler() {
  worker_thread_.request_stop();

  if (epoll_fd_ != -1) {
    close(epoll_fd_);
  }
}

// Main operation loop

void TsScheduler::operator()(std::stop_token stop_token) {
  cbm::system::set_thread_name("TsScheduler");

  if (!ucx::util::init(context_, worker_, epoll_fd_)) {
    ERROR("Failed to initialize UCX");
    return;
  }
  if (!ucx::util::set_receive_handler(worker_, AM_SENDER_REGISTER,
                                      on_sender_register, this) ||
      !ucx::util::set_receive_handler(worker_, AM_SENDER_ANNOUNCE_ST,
                                      on_sender_announce, this) ||
      !ucx::util::set_receive_handler(worker_, AM_SENDER_RETRACT_ST,
                                      on_sender_retract, this) ||
      !ucx::util::set_receive_handler(worker_, AM_BUILDER_REGISTER,
                                      on_builder_register, this) ||
      !ucx::util::set_receive_handler(worker_, AM_BUILDER_STATUS,
                                      on_builder_status, this)) {
    ERROR("Failed to register receive handlers");
    return;
  }
  if (!ucx::util::create_listener(worker_, listener_, listen_port_,
                                  on_new_connection, this)) {
    ERROR("Failed to create UCX listener");
    return;
  }

  uint64_t id = fles::system::current_time_ns() / timeslice_duration_ns_;
  report_status();

  while (!stop_token.stop_requested()) {
    if (ucp_worker_progress(worker_) != 0) {
      continue;
    }
    tasks_.timer();

    bool try_later =
        senders_.empty() ||
        std::any_of(senders_.begin(), senders_.end(), [id](const auto& s) {
          return s.second.last_received_st < id;
        });
    uint64_t current_time_ns = fles::system::current_time_ns();
    uint64_t timeout_ns = (id + 1) * timeslice_duration_ns_ + timeout_ns_;

    if (try_later && current_time_ns < timeout_ns) {
      int sender_wait_ms =
          static_cast<int>((timeout_ns - current_time_ns + 999999) / 1000000);
      int timer_wait_ms = std::max(
          static_cast<int>(
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  tasks_.when_next() - std::chrono::system_clock::now())
                  .count() +
              1),
          0);
      int timeout_ms = std::min(sender_wait_ms, timer_wait_ms);
      if (!ucx::util::arm_worker_and_wait(worker_, epoll_fd_, timeout_ms)) {
        break;
      }
      continue;
    }

    send_timeslice(id);
    id++;
  }

  if (listener_ != nullptr) {
    ucp_listener_destroy(listener_);
    listener_ = nullptr;
  }
  disconnect_from_all();
  ucx::util::cleanup(context_, worker_);
}

void TsScheduler::send_timeslice(TsId id) {
  StCollection coll = create_collection_descriptor(id);
  DEBUG("Processing timeslice {}: {}", id, coll);
  if (coll.sender_ids.empty()) {
    WARN("No contributions found for timeslice {}", id);
    return;
  }

  for (std::size_t i = 0; i < builders_.size(); ++i) {
    auto& builder = builders_[(id + i) % builders_.size()];
    if (builder.bytes_available >= coll.ms_data_size() &&
        !builder.is_out_of_memory) {
      send_timeslice_to_builder(coll, builder);
      return;
    }
  }
  WARN("No builder available for timeslice {}", id);
  send_release_to_senders(id);
}

// Connection management

void TsScheduler::handle_new_connection(ucp_conn_request_h conn_request) {
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

void TsScheduler::handle_endpoint_error(ucp_ep_h ep, ucs_status_t status) {
  ERROR("Error on UCX endpoint: {}", status);

  auto it = connections_.find(ep);
  if (it != connections_.end()) {
    INFO("Removing disconnected endpoint '{}'", it->second);
    connections_.erase(it);
  } else {
    ERROR("Received error for unknown endpoint");
  }

  auto sender_it = senders_.find(ep);
  if (sender_it != senders_.end()) {
    INFO("Removing disconnected sender '{}'", sender_it->second.sender_id);
    senders_.erase(sender_it);
  }

  if (auto it =
          std::find_if(builders_.begin(), builders_.end(),
                       [ep](const BuilderConnection& b) { return b.ep == ep; });
      it != builders_.end()) {
    DEBUG("Removing disconnected builder '{}'", it->id);
    builders_.erase(it);
  }

  // Fail any in-progress timeslice allocations that involved this endpoint
  // (TODO: not implemented yet)
}

void TsScheduler::disconnect_from_all() {
  for (auto& [ep, _] : connections_) {
    ucx::util::close_endpoint(worker_, ep, true);
  }
  INFO("Disconnected from all senders and builders");

  connections_.clear();
  senders_.clear();
  builders_.clear();
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

  auto sender_id = std::string(static_cast<const char*>(header), header_length);
  ucp_ep_h ep = param->reply_ep;
  senders_[ep] = {sender_id, ep, {}};
  DEBUG("Accepted sender registration from '{}'", sender_id);

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

  auto it = senders_.find(param->reply_ep);
  if (it == senders_.end()) {
    ERROR("Received announcement from unknown sender");
    return UCS_OK;
  }
  auto& sender_conn = it->second;

  // StDescriptor can be used for statistics in the future, ignored for now
  auto st_descriptor_bytes =
      std::span(static_cast<const std::byte*>(data), length);
  std::vector<std::byte> st_descriptor_vector(st_descriptor_bytes.begin(),
                                              st_descriptor_bytes.end());

  sender_conn.announced_st.emplace_back(id, ms_data_size);
  sender_conn.last_received_st = id;
  DEBUG("Received subtimeslice announcement from sender '{}' for {}, "
        "ms_data_size: {}",
        sender_conn.sender_id, id, ms_data_size);

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

  auto it = senders_.find(param->reply_ep);
  if (it == senders_.end()) {
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

  for (auto& [ep, conn] : senders_) {
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

  auto builder_id =
      std::string(static_cast<const char*>(header), header_length);
  ucp_ep_h ep = param->reply_ep;
  builders_.emplace_back(builder_id, ep, 0, false);
  DEBUG("Accepted builder registration from '{}'", builder_id);

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
  auto it = std::find_if(builders_.begin(), builders_.end(),
                         [ep = param->reply_ep](const BuilderConnection& b) {
                           return b.ep == ep;
                         });
  if (it == builders_.end()) {
    ERROR("Received status from unknown builder");
    return UCS_OK;
  }
  const uint64_t event = hdr[0];
  const TsId id = hdr[1];
  const uint64_t new_bytes_free = hdr[2];

  switch (event) {
  case BUILDER_EVENT_NO_OP:
    DEBUG("Builder '{}' reported bytes free: {}", it->id, new_bytes_free);
    if (new_bytes_free > it->bytes_available) {
      it->is_out_of_memory = false;
    }
    it->bytes_available = new_bytes_free;
    break;
  case BUILDER_EVENT_ALLOCATED:
    DEBUG("Builder '{}' has allocated timeslice {}, new bytes free: {}", it->id,
          id, new_bytes_free);
    it->bytes_available = new_bytes_free;
    break;
  case BUILDER_EVENT_OUT_OF_MEMORY:
    INFO("Builder '{}' reported out of memory for timeslice {}", it->id, id);
    it->is_out_of_memory = true;
    send_timeslice(id);
    break;
  case BUILDER_EVENT_RECEIVED:
    DEBUG("Builder '{}' has received timeslice {}", it->id, id);
    send_release_to_senders(id);
    break;
  case BUILDER_EVENT_RELEASED:
    DEBUG("Builder '{}' has released timeslice {}, new bytes free: {}", it->id,
          id, new_bytes_free);
    if (new_bytes_free > it->bytes_available) {
      it->is_out_of_memory = false;
    }
    it->bytes_available = new_bytes_free;
    break;
  }

  return UCS_OK;
}

void TsScheduler::send_timeslice_to_builder(const StCollection& coll,
                                            BuilderConnection& builder) {
  std::array<uint64_t, 2> hdr{coll.id, coll.ms_data_size()};
  auto header = std::as_bytes(std::span(hdr));
  auto buffer = std::make_unique<std::vector<std::byte>>(to_bytes(coll));
  auto* raw_ptr = buffer.release();

  ucx::util::send_active_message(
      builder.ep, AM_SCHED_SEND_TS, header, *raw_ptr,
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
  for (auto& [sender_ep, sender] : senders_) {
    auto it =
        std::find_if(sender.announced_st.begin(), sender.announced_st.end(),
                     [id](const auto& st) { return st.id == id; });
    if (it != sender.announced_st.end()) {
      TRACE("Adding contribution from sender '{}' for {}, ms_data_size: {}",
            sender.sender_id, id, it->ms_data_size);
      coll.sender_ids.push_back(sender.sender_id);
      coll.ms_data_sizes.push_back(it->ms_data_size);
      sender.announced_st.erase(it);
    } else {
      DEBUG("No contribution found from sender '{}' for {}", sender.sender_id,
            id);
    }
  }
  return coll;
}

void TsScheduler::report_status() {
  constexpr auto interval = std::chrono::seconds(1);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  if (monitor_ != nullptr) {
    monitor_->QueueMetric("tssched_status", {{"host", hostname_}},
                          {{"timeslice_count", 0}});
  }
  // TODO: Add real metrics

  tasks_.add([this] { report_status(); }, now + interval);
}
