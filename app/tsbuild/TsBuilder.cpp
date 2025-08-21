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

namespace {
inline uint64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}
} // namespace

TsBuilder::TsBuilder(TimesliceBufferFlex& timeslice_buffer,
                     std::string_view scheduler_address,
                     int64_t timeout_ns)
    : timeslice_buffer_(timeslice_buffer),
      scheduler_address_(scheduler_address), timeout_ns_{timeout_ns} {
  builder_id_ = fles::system::current_hostname() + ":" +
                std::to_string(fles::system::current_pid()); // TODO

  // Initialize event handling
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ == -1) {
    throw std::runtime_error("epoll_create1 failed");
  }

  // Start the worker thread
  worker_thread_ = std::jthread([this](std::stop_token st) { (*this)(st); });
}

TsBuilder::~TsBuilder() {
  worker_thread_.request_stop();

  if (epoll_fd_ != -1) {
    close(epoll_fd_);
  }
}

// Main operation loop

void TsBuilder::operator()(std::stop_token stop_token) {
  cbm::system::set_thread_name("TsBuilder");

  if (!ucx::util::init(context_, worker_, epoll_fd_)) {
    L_(error) << "Failed to initialize UCX";
    return;
  }
  if (!ucx::util::set_receive_handler(worker_, AM_SCHED_SEND_TS,
                                      on_scheduler_send_ts, this) ||
      !ucx::util::set_receive_handler(worker_, AM_SENDER_SEND_ST,
                                      on_sender_data, this)) {
    L_(error) << "Failed to register receive handlers";
    return;
  }
  connect_to_scheduler_if_needed();

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

  disconnect_from_scheduler();
  disconnect_from_senders();
  ucx::util::cleanup(context_, worker_);
}

// Scheduler connection management

void TsBuilder::connect_to_scheduler_if_needed() {
  constexpr auto interval = std::chrono::seconds(2);
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  if (!scheduler_connecting_ &&
      !worker_thread_.get_stop_token().stop_requested()) {
    connect_to_scheduler();
  }

  tasks_.add([this] { connect_to_scheduler_if_needed(); }, now + interval);
}

void TsBuilder::connect_to_scheduler() {
  if (scheduler_connecting_ || scheduler_connected_) {
    return;
  }

  auto [address, port] =
      ucx::util::parse_address(scheduler_address_, DEFAULT_SCHEDULER_PORT);
  auto ep =
      ucx::util::connect(worker_, address, port, on_scheduler_error, this);
  if (!ep) {
    L_(error) << "Failed to connect to scheduler at " << address << ":" << port;
    return;
  }

  scheduler_ep_ = *ep;

  if (!register_with_scheduler()) {
    L_(error) << "Failed to register with scheduler";
    disconnect_from_scheduler(true);
    return;
  }

  scheduler_connecting_ = true;
}

void TsBuilder::handle_scheduler_error(ucp_ep_h ep, ucs_status_t status) {
  if (ep != scheduler_ep_) {
    L_(error) << "Received error for unknown endpoint: "
              << ucs_status_string(status);
    return;
  }

  disconnect_from_scheduler(true);
  L_(info) << "Disconnected from scheduler: " << ucs_status_string(status);
}

bool TsBuilder::register_with_scheduler() {
  auto header = std::as_bytes(std::span(builder_id_));
  return ucx::util::send_active_message(
      scheduler_ep_, AM_SENDER_REGISTER, header, {},
      on_scheduler_register_complete, this, 0);
}

void TsBuilder::handle_scheduler_register_complete(ucs_status_ptr_t request,
                                                   ucs_status_t status) {
  scheduler_connecting_ = false;

  if (status != UCS_OK) {
    L_(error) << "Failed to register with scheduler: "
              << ucs_status_string(status);
  } else {
    scheduler_connected_ = true;
    L_(info) << "Successfully registered with scheduler";
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
};

void TsBuilder::disconnect_from_scheduler(bool force) {
  scheduler_connecting_ = false;
  scheduler_connected_ = false;

  if (scheduler_ep_ == nullptr) {
    return;
  }

  ucx::util::close_endpoint(worker_, scheduler_ep_, force);
  scheduler_ep_ = nullptr;
  L_(debug) << "Disconnected from scheduler";
}

// Scheduler message handling

void TsBuilder::send_status_to_scheduler() {
  std::array<uint64_t, 2> hdr{bytes_available_, bytes_processed_}; // TODO

  auto header = std::as_bytes(std::span(hdr));

  ucx::util::send_active_message(scheduler_ep_, AM_BUILDER_STATUS, header, {},
                                 ucx::util::on_generic_send_complete, this,
                                 UCP_AM_SEND_FLAG_COPY_HEADER);
}

ucs_status_t
TsBuilder::handle_scheduler_send_ts(const void* header,
                                    size_t header_length,
                                    void* data,
                                    size_t length,
                                    const ucp_am_recv_param_t* param) {
  L_(trace) << "Received TS from scheduler with header length " << header_length
            << " and data length " << length;
  /* TODO
    auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                         header_length / sizeof(uint64_t));
    if (hdr.size() != 3 || length == 0 ||
        (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
      L_(error) << "Invalid scheduler TS received";
      return UCS_OK;
    }

    const uint64_t id = hdr[0];
    const uint64_t desc_size = hdr[1];
    const uint64_t content_size = hdr[2];

    if (desc_size != length) {
      L_(error) << "Invalid header data in scheduler TS";
      return UCS_OK;
    }

    auto it = sender_connections_.find(param->reply_ep);
    if (it == sender_connections_.end()) {
      L_(error) << "Received announcement from unknown sender";
      return UCS_OK;
    }
    auto& sender_conn = it->second;

    // descriptor can be used for statistics in the future, ignored for now
    auto descriptor_bytes =
        std::span(static_cast<const std::byte*>(data), desc_size);
    std::vector<std::byte> descriptor_vector(descriptor_bytes.begin(),
                                             descriptor_bytes.end());

    sender_conn.announced_st.emplace_back(id, desc_size, content_size);
    sender_conn.last_received_st = id;
    L_(debug) << "Received ST announcement from sender " << sender_conn.id
              << " with ID: " << id << ", desc size: " << desc_size
              << ", content size: " << content_size;
  */
  return UCS_OK;
}

// Sender connection management

void TsBuilder::connect_to_sender(std::string sender_id) {
  auto [address, port] =
      ucx::util::parse_address(sender_id, DEFAULT_SENDER_PORT);
  auto ep =
      ucx::util::connect(worker_, address, port, on_scheduler_error, this);
  if (!ep) {
    L_(error) << "Failed to connect to sender at " << address << ":" << port;
    return;
  }

  senders_.emplace_back(sender_id, *ep);
}

void TsBuilder::handle_sender_error(ucp_ep_h ep, ucs_status_t status) {
  auto it =
      std::find_if(senders_.begin(), senders_.end(),
                   [ep](const SenderConnection& s) { return s.ep == ep; });
  if (it == senders_.end()) {
    L_(error) << "Received error for unknown sender endpoint: "
              << ucs_status_string(status);
    return;
  }
  ucx::util::close_endpoint(worker_, it->ep, true);
  senders_.erase(it);
  L_(info) << "Sender " << it->id
           << " disconnected: " << ucs_status_string(status);
}

void TsBuilder::disconnect_from_senders() {
  for (auto& sender : senders_) {
    ucx::util::close_endpoint(worker_, sender.ep, true);
  }
  senders_.clear();
  L_(info) << "Disconnected from all senders";
}

// Sender message handling

void TsBuilder::send_request_to_sender(StID id) {
  std::array<uint64_t, 1> hdr{id};

  auto header = std::as_bytes(std::span(hdr));

  ucx::util::send_active_message(scheduler_ep_, AM_BUILDER_REQUEST_ST, header,
                                 {}, ucx::util::on_generic_send_complete, this,
                                 UCP_AM_SEND_FLAG_COPY_HEADER);
}

ucs_status_t TsBuilder::handle_sender_data(const void* header,
                                           size_t header_length,
                                           void* data,
                                           size_t length,
                                           const ucp_am_recv_param_t* param) {
  L_(trace) << "Received sender ST data with header length " << header_length
            << " and data length " << length;
  /* TODO
    auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                         header_length / sizeof(uint64_t));
    if (hdr.size() != 3 || length == 0 ||
        (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
      L_(error) << "Invalid sender ST announcement received";
      return UCS_OK;
    }

    const uint64_t id = hdr[0];
    const uint64_t desc_size = hdr[1];
    const uint64_t content_size = hdr[2];

    if (desc_size != length) {
      L_(error) << "Invalid header data in sender ST announcement";
      return UCS_OK;
    }

    auto it = sender_connections_.find(param->reply_ep);
    if (it == sender_connections_.end()) {
      L_(error) << "Received announcement from unknown sender";
      return UCS_OK;
    }
    auto& sender_conn = it->second;

    // descriptor can be used for statistics in the future, ignored for now
    auto descriptor_bytes =
        std::span(static_cast<const std::byte*>(data), desc_size);
    std::vector<std::byte> descriptor_vector(descriptor_bytes.begin(),
                                             descriptor_bytes.end());

    sender_conn.announced_st.emplace_back(id, desc_size, content_size);
    sender_conn.last_received_st = id;
    L_(debug) << "Received ST announcement from sender " << sender_conn.id
              << " with ID: " << id << ", desc size: " << desc_size
              << ", content size: " << content_size;
  */
  return UCS_OK;
}

// Queue processing

std::size_t TsBuilder::process_queues() {
  // TODO
  return 0;
}

// Helper methods

TsDescriptor TsBuilder::create_timeslice_descriptor(StID id) {
  // TODO

  return {};
}
