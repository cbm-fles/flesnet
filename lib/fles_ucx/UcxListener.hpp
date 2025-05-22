// Copyright 2023-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "UcxContext.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class UcxListener {
public:
  using ConnectionCallback =
      std::function<void(ucp_conn_request_h, const struct sockaddr*)>;

  UcxListener(UcxContext& context, uint16_t port, ConnectionCallback callback);
  ~UcxListener();

  UcxListener(const UcxListener&) = delete;
  UcxListener& operator=(const UcxListener&) = delete;

  /// Accept a connection request
  ucp_ep_h accept(ucp_conn_request_h conn_request);

  /// Reject a connection request
  void reject(ucp_conn_request_h conn_request);

private:
  UcxContext& context_;
  ucp_listener_h listener_ = nullptr;
  ConnectionCallback callback_;

  static void connection_handler(ucp_conn_request_h conn_request, void* arg);
};

class UcxConnectionManager {
public:
  using NewConnectionCallback = std::function<void(ucp_ep_h, uint16_t)>;

  UcxConnectionManager(UcxContext& context,
                       uint16_t port,
                       uint32_t expected_connections);
  ~UcxConnectionManager();

  UcxConnectionManager(const UcxConnectionManager&) = delete;
  UcxConnectionManager& operator=(const UcxConnectionManager&) = delete;

  /// Set callback for new connections
  void set_connection_callback(NewConnectionCallback callback) {
    connection_callback_ = callback;
  }

  /// Wait for all expected connections
  bool wait_for_all_connections(int timeout_ms = -1);

  /// Get number of active connections
  [[nodiscard]] size_t connection_count() const { return connections_.size(); }

private:
  UcxContext& context_;
  std::unique_ptr<UcxListener> listener_;
  std::vector<ucp_ep_h> connections_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<uint32_t> connected_{0};
  uint32_t expected_connections_;
  NewConnectionCallback connection_callback_;

  void on_connection(ucp_conn_request_h conn_request,
                     const struct sockaddr* client_addr);
};
