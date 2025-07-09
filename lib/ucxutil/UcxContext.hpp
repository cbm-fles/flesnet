// Copyright 2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <stdexcept>
#include <string>
#include <ucp/api/ucp.h>

class UcxException : public std::runtime_error {
public:
  explicit UcxException(const std::string& what_arg = "")
      : std::runtime_error(what_arg) {}
};

/// UCP context and worker wrapper class
class UcpContext {
public:
  /// Constructor initializes the UCP context and worker
  UcpContext();

  /// Destructor cleans up UCP resources
  ~UcpContext();

  UcpContext(const UcpContext&) = delete;
  UcpContext& operator=(const UcpContext&) = delete;

  /// Get the UCX context
  [[nodiscard]] ucp_context_h context() const { return ucp_context_; }

  /// Get the UCX worker
  [[nodiscard]] ucp_worker_h worker() const { return ucp_worker_; }

  /// Progress the UCX worker
  void progress() { ucp_worker_progress(ucp_worker_); }

private:
  ucp_context_h ucp_context_ = nullptr;
  ucp_worker_h ucp_worker_ = nullptr;
};
