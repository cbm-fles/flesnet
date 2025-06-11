// Copyright 2023-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "UcxContext.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <string>

class UcxConnection {
public:
  using CompletionCallback = std::function<void(ucs_status_t)>;

  UcxConnection(UcpContext& context);

  virtual ~UcxConnection();

  UcxConnection(const UcxConnection&) = delete;
  UcxConnection& operator=(const UcxConnection&) = delete;

  /// Connect to a remote endpoint
  void connect(const std::string& hostname, uint16_t port);

  /// Disconnect from remote endpoint
  void disconnect();

  /// Check if connection is established
  [[nodiscard]] bool is_connected() const { return connected_; }

  /// Get the endpoint
  [[nodiscard]] ucp_ep_h endpoint() const { return ep_; }

  /// Progress the connection
  void progress();

  /// Send data with active message semantics
  void send_am(const void* buffer, size_t length);

protected:
  UcpContext& context_;
  ucp_ep_h ep_ = nullptr;
  std::atomic<bool> connected_{false};
  std::atomic<bool> disconnecting_{false};
  std::promise<void> connect_promise_;

  static void err_handler_cb(void* arg, ucp_ep_h ep, ucs_status_t status);
  static void send_am_cb(void* request, ucs_status_t status, void* user_data);
};
