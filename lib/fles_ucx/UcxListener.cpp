// Copyright 2023-2025 Jan de Cuveland <cmail@cuveland.de>

#include "UcxListener.hpp"
#include "log.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>

UcxListener::UcxListener(UcxContext& context,
                         uint16_t port,
                         ConnectionCallback callback)
    : context_(context), callback_(callback) {

  struct sockaddr_in listen_addr {};
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  listen_addr.sin_port = htons(port);

  ucp_listener_params_t params{};
  params.field_mask = UCP_LISTENER_PARAM_FIELD_SOCK_ADDR |
                      UCP_LISTENER_PARAM_FIELD_CONN_HANDLER;
  params.sockaddr.addr = reinterpret_cast<const struct sockaddr*>(&listen_addr);
  params.sockaddr.addrlen = sizeof(listen_addr);
  params.conn_handler.cb = connection_handler;
  params.conn_handler.arg = this;

  ucs_status_t status =
      ucp_listener_create(context.worker(), &params, &listener_);
  if (status != UCS_OK) {
    throw UcxException("Failed to create UCX listener");
  }

  L_(info) << "UCX listener created on port " << port;
}

UcxListener::~UcxListener() {
  if (listener_ != nullptr) {
    ucp_listener_destroy(listener_);
  }
}

ucp_ep_h UcxListener::accept(ucp_conn_request_h conn_request) {
  ucp_ep_h ep;
  ucp_ep_params_t ep_params;
  memset(&ep_params, 0, sizeof(ep_params));

  ep_params.field_mask = UCP_EP_PARAM_FIELD_CONN_REQUEST;
  ep_params.conn_request = conn_request;

  ucs_status_t status = ucp_ep_create(context_.worker(), &ep_params, &ep);
  if (status != UCS_OK) {
    L_(error) << "Failed to create endpoint from connection request";
    return nullptr;
  }

  return ep;
}

void UcxListener::reject(ucp_conn_request_h conn_request) {
  ucp_listener_reject(listener_, conn_request);
}

void UcxListener::connection_handler(ucp_conn_request_h conn_request,
                                     void* arg) {
  auto* listener = static_cast<UcxListener*>(arg);

  // Get client address
  const struct sockaddr* client_addr;
  ucp_conn_request_attr_t attr{};
  attr.field_mask = UCP_CONN_REQUEST_ATTR_FIELD_CLIENT_ADDR;

  ucs_status_t status = ucp_conn_request_query(conn_request, &attr);
  if (status != UCS_OK) {
    L_(error) << "Failed to query connection request";
    listener->reject(conn_request);
    return;
  }

  client_addr = reinterpret_cast<const struct sockaddr*>(&attr.client_address);

  // Call user callback
  if (listener->callback_) {
    listener->callback_(conn_request, client_addr);
  } else {
    listener->reject(conn_request);
  }
}

UcxConnectionManager::UcxConnectionManager(UcxContext& context,
                                           uint16_t port,
                                           uint32_t expected_connections)
    : context_(context), expected_connections_(expected_connections) {

  listener_ = std::make_unique<UcxListener>(
      context, port,
      [this](ucp_conn_request_h request, const struct sockaddr* addr) {
        this->on_connection(request, addr);
      });
}

UcxConnectionManager::~UcxConnectionManager() {
  // Close all endpoints
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto* ep : connections_) {
    if (ep != nullptr) {
      ucp_ep_close_nb(ep, UCP_EP_CLOSE_MODE_FORCE);
      // We don't wait for completion here to avoid blocking in destructor
    }
  }
}

bool UcxConnectionManager::wait_for_all_connections(int timeout_ms) {
  std::unique_lock<std::mutex> lock(mutex_);

  if (timeout_ms < 0) {
    // Wait indefinitely
    cv_.wait(lock, [this]() { return connected_ >= expected_connections_; });
    return true;
  } // Wait with timeout
  return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                      [this]() { return connected_ >= expected_connections_; });
}

void UcxConnectionManager::on_connection(ucp_conn_request_h conn_request,
                                         const struct sockaddr* client_addr) {
  char client_ip[INET6_ADDRSTRLEN];
  uint16_t client_port = 0;

  // Extract client IP and port
  if (client_addr->sa_family == AF_INET) {
    const auto* addr_in =
        reinterpret_cast<const struct sockaddr_in*>(client_addr);
    inet_ntop(AF_INET, &addr_in->sin_addr, static_cast<char*>(client_ip),
              sizeof(client_ip));
    client_port = ntohs(addr_in->sin_port);
  } else if (client_addr->sa_family == AF_INET6) {
    const auto* addr_in6 =
        reinterpret_cast<const struct sockaddr_in6*>(client_addr);
    inet_ntop(AF_INET6, &addr_in6->sin6_addr, static_cast<char*>(client_ip),
              sizeof(client_ip));
    client_port = ntohs(addr_in6->sin6_port);
  }

  L_(debug) << "Connection request from " << client_ip << ":" << client_port;

  // Accept the connection
  ucp_ep_h ep = listener_->accept(conn_request);
  if (ep != nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.push_back(ep);
    connected_++;

    if (connection_callback_) {
      connection_callback_(ep, connections_.size() - 1);
    }

    L_(info) << "Connection established: " << connected_ << "/"
             << expected_connections_;

    if (connected_ >= expected_connections_) {
      cv_.notify_all();
    }
  } else {
    listener_->reject(conn_request);
  }
}
