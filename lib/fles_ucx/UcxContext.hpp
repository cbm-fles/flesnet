// Copyright 2023-2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <stdexcept>
#include <string>
#include <ucp/api/ucp.h>

class UcxException : public std::runtime_error {
public:
  explicit UcxException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};

/// UCX context wrapper class
class UcxContext {
public:
  /// Constructor initializes the UCX context and worker
  UcxContext();

  /// Destructor cleans up UCX resources
  ~UcxContext();

  UcxContext(const UcxContext&) = delete;
  UcxContext& operator=(const UcxContext&) = delete;

  /// Get the UCX context
  [[nodiscard]] ucp_context_h context() const { return ucp_context_; }

  /// Get the UCX worker
  [[nodiscard]] ucp_worker_h worker() const { return ucp_worker_; }

  /// Progress the UCX worker
  void progress();

  /// Wait for UCX events
  void wait();

private:
  ucp_context_h ucp_context_ = nullptr;
  ucp_worker_h ucp_worker_ = nullptr;
};

/// Memory region wrapper for UCX memory mapping
class UcxMemoryRegion {
public:
  UcxMemoryRegion(UcxContext& context,
                  void* addr,
                  size_t length,
                  unsigned flags);
  ~UcxMemoryRegion();

  UcxMemoryRegion(const UcxMemoryRegion&) = delete;
  UcxMemoryRegion& operator=(const UcxMemoryRegion&) = delete;

  /// Get memory handle for remote access
  [[nodiscard]] ucp_mem_h handle() const { return memh_; }

  /// Get memory region key for remote access
  [[nodiscard]] const void* rkey_buffer() const { return &rkey_buffer_; }

private:
  UcxContext& context_;
  ucp_mem_h memh_ = nullptr;
  void* rkey_buffer_ = nullptr;
  size_t rkey_length_ = 0;
};
