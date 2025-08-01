// Copyright 2025 Jan de Cuveland

#include "TsScheduler.hpp"
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

TsScheduler::TsScheduler(uint16_t listen_port) : listen_port_(listen_port) {
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
    L_(error) << "Failed to initialize UCX";
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
    L_(error) << "Failed to register receive handlers";
    return;
  }
  if (!ucx::util::create_listener(worker_, listener_, listen_port_,
                                  on_new_connection, this)) {
    L_(error) << "Failed to create UCX listener";
    return;
  }

  while (!stop_token.stop_requested()) {
    if (ucp_worker_progress(worker_) != 0) {
      continue;
    }
    if (!ucx::util::arm_worker_and_wait(worker_, epoll_fd_)) {
      break;
    }
  }

  if (listener_ != nullptr) {
    ucp_listener_destroy(listener_);
    listener_ = nullptr;
  }
  ucx::util::cleanup(context_, worker_);
}

// Connection management

void TsScheduler::handle_new_connection(ucp_conn_request_h conn_request) {
  L_(debug) << "New connection request received";

  auto client_address = ucx::util::get_client_address(conn_request);
  if (!client_address) {
    L_(error) << "Failed to retrieve client address from connection request";
    ucp_listener_reject(listener_, conn_request);
    return;
  }

  auto ep = ucx::util::accept(worker_, conn_request, on_endpoint_error, this);
  if (!ep) {
    L_(error) << "Failed to create endpoint for new connection";
    return;
  }

  connections_[*ep] = *client_address;
  L_(debug) << "Accepted connection from " << *client_address;
}

void TsScheduler::handle_endpoint_error(ucp_ep_h ep, ucs_status_t status) {
  L_(error) << "Error on UCX endpoint: " << ucs_status_string(status);

  auto it = connections_.find(ep);
  if (it != connections_.end()) {
    L_(info) << "Removing disconnected endpoint: " << it->second;
    connections_.erase(it);
  } else {
    L_(error) << "Received error for unknown endpoint";
  }

  auto sender_it = sender_connections_.find(ep);
  if (sender_it != sender_connections_.end()) {
    L_(info) << "Removing disconnected sender: " << sender_it->second.id;
    sender_connections_.erase(sender_it);
  }

  auto builder_it = builder_connections_.find(ep);
  if (builder_it != builder_connections_.end()) {
    L_(info) << "Removing disconnected builder: " << builder_it->second.id;
    builder_connections_.erase(builder_it);
  }
}

// Sender message handling

ucs_status_t
TsScheduler::handle_sender_register(const void* header,
                                    size_t header_length,
                                    [[maybe_unused]] void* data,
                                    size_t length,
                                    const ucp_am_recv_param_t* param) {
  L_(trace) << "Received sender registration with header length "
            << header_length << " and data length " << length;

  if (header_length == 0 || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    L_(error) << "Invalid sender registration request received";
    return UCS_OK;
  }

  auto sender_id = std::string(static_cast<const char*>(header), header_length);
  ucp_ep_h ep = param->reply_ep;
  sender_connections_[ep] = {sender_id, ep, {}};
  L_(debug) << "Accepted sender registration with id " << sender_id;

  return UCS_OK;
}

ucs_status_t
TsScheduler::handle_sender_announce(const void* header,
                                    size_t header_length,
                                    void* data,
                                    size_t length,
                                    const ucp_am_recv_param_t* param) {
  L_(trace) << "Received sender ST announcement with header length "
            << header_length << " and data length " << length;

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
  L_(debug) << "Received ST announcement from sender " << sender_conn.id
            << " with ID: " << id << ", desc size: " << desc_size
            << ", content size: " << content_size;

  return UCS_OK;
}

ucs_status_t
TsScheduler::handle_sender_retract(const void* header,
                                   size_t header_length,
                                   [[maybe_unused]] void* data,
                                   size_t length,
                                   const ucp_am_recv_param_t* param) {
  L_(trace) << "Received sender ST retraction with header length "
            << header_length << " and data length " << length;

  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 1 || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    L_(error) << "Invalid sender ST retraction received";
    return UCS_OK;
  }

  const uint64_t id = hdr[0];

  auto it = sender_connections_.find(param->reply_ep);
  if (it == sender_connections_.end()) {
    L_(error) << "Received retraction from unknown sender";
    return UCS_OK;
  }
  auto& conn = it->second;

  std::erase_if(conn.announced_st,
                [id](const auto& st) { return st.id == id; });

  return UCS_OK;
}

void TsScheduler::send_release_to_senders(StID id) {
  std::array<uint64_t, 1> hdr{id};
  auto header = std::as_bytes(std::span(hdr));

  for (auto& [ep, conn] : sender_connections_) {
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
  L_(trace) << "Received builder registration with header length "
            << header_length << " and data length " << length;

  if (header_length == 0 || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    L_(error) << "Invalid builder registration request received";
    return UCS_OK;
  }

  auto sender_id = std::string(static_cast<const char*>(header), header_length);
  ucp_ep_h ep = param->reply_ep;
  builder_connections_[ep] = {sender_id, ep, 0, 0, 0};
  L_(debug) << "Accepted builder registration with id " << sender_id;

  return UCS_OK;
}

ucs_status_t
TsScheduler::handle_builder_status(const void* header,
                                   size_t header_length,
                                   [[maybe_unused]] void* data,
                                   size_t length,
                                   const ucp_am_recv_param_t* param) {
  L_(trace) << "Received builder status with header length " << header_length
            << " and data length " << length;

  auto hdr = std::span<const uint64_t>(static_cast<const uint64_t*>(header),
                                       header_length / sizeof(uint64_t));
  if (hdr.size() != 2 || length != 0 ||
      (param->recv_attr & UCP_AM_RECV_ATTR_FIELD_REPLY_EP) == 0u) {
    L_(error) << "Invalid builder status received";
    return UCS_OK;
  }

  auto it = builder_connections_.find(param->reply_ep);
  if (it == builder_connections_.end()) {
    L_(error) << "Received status from unknown builder";
    return UCS_OK;
  }
  auto& conn = it->second;

  const uint64_t bytes_available = hdr[0];
  const uint64_t bytes_processed = hdr[1];

  conn.bytes_available = bytes_available;
  conn.bytes_processed = bytes_processed;

  return UCS_OK;
}

void TsScheduler::send_timeslice_to_builder(StID id, ucp_ep_h ep) {
  // TODO ...
}

void TsScheduler::handle_builder_send_complete(void* request,
                                               ucs_status_t status) {
  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Send operation failed: " << ucs_status_string(status);
  } else if (status != UCS_OK) {
    L_(error) << "Send operation completed with status: "
              << ucs_status_string(status);
  } else {
    L_(trace) << "Send operation completed successfully";
  }

  auto it = active_send_requests_.find(request);
  if (it == active_send_requests_.end()) {
    L_(error) << "Received completion for unknown send request";
  } else {
    StID id = it->second;
    active_send_requests_.erase(request);
    // TODO
    // release the metadata for the timeslice with the given id
    // complete_timeslice(id);
    // announced_.erase(id);
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
}
