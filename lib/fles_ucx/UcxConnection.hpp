// Copyright 2023-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "UcxContext.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <string>

struct UcxConnectionInfo {
  ucp_address_t* address;
  size_t address_length;
  uint64_t connection_id;
};

class UcxConnection {
public:
  using CompletionCallback = std::function<void(ucs_status_t)>;

  UcxConnection(UcxContext& context,
                uint_fast16_t connection_index,
                uint_fast16_t remote_connection_index);

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

  /// Get the connection index
  [[nodiscard]] uint_fast16_t index() const { return index_; }

  /// Get the remote connection index
  [[nodiscard]] uint_fast16_t remote_index() const { return remote_index_; }

  /// Progress the connection
  void progress();

  /// Flush all outstanding operations
  void flush();

  /// Send data with tag-based matching
  void send_tagged(const void* buffer,
                   size_t length,
                   uint64_t tag,
                   CompletionCallback callback);

  /// Receive data with tag-based matching
  void recv_tagged(void* buffer,
                   size_t length,
                   uint64_t tag,
                   CompletionCallback callback);

  /// Perform remote memory access write
  void rma_put(const void* buffer,
               size_t length,
               uint64_t remote_addr,
               ucp_rkey_h rkey,
               CompletionCallback callback);

  /// Perform remote memory access read
  void rma_get(void* buffer,
               size_t length,
               uint64_t remote_addr,
               ucp_rkey_h rkey,
               CompletionCallback callback);

protected:
  UcxContext& context_;
  uint_fast16_t index_;
  uint_fast16_t remote_index_;
  ucp_ep_h ep_ = nullptr;
  std::atomic<bool> connected_{false};
  std::atomic<bool> disconnecting_{false};
  std::promise<void> connect_promise_;

  static void
  send_callback(void* request, ucs_status_t status, void* user_data);
  static void recv_callback(void* request,
                            ucs_status_t status,
                            const ucp_tag_recv_info_t* info,
                            void* user_data);
  static void rma_callback(void* request, ucs_status_t status, void* user_data);
  static void ep_close_callback(void* request, ucs_status_t status, void* arg);
};
