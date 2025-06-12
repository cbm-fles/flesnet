// Copyright 2025 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "UcxConnection.hpp"
#include "UcxContext.hpp"
#include <functional>

/// UCP listener wrapper class
class UcpListener {
public:
  UcpListener(UcpContext& context,
              uint16_t port,
              void* recv_buffer_data,
              size_t recv_buffer_size);
  ~UcpListener();

  UcpListener(const UcpListener&) = delete;
  UcpListener& operator=(const UcpListener&) = delete;

  void conn_callback(ucp_conn_request_h conn_request);

  // private:
  UcpContext& context_;
  ucp_listener_h listener_ = nullptr;

  void* recv_buffer_data_;
  size_t recv_buffer_size_;

  std::vector<ucp_ep_h> connections_;

  static void static_conn_callback(ucp_conn_request_h conn_request, void* arg);

  static void err_handler_cb(void* arg, ucp_ep_h ep, ucs_status_t status);
  static ucs_status_t am_recv_callback(void* arg,
                                       const void* header,
                                       size_t header_length,
                                       void* data,
                                       size_t length,
                                       const ucp_am_recv_param_t* param);
  static void am_recv_data_nbx_callback(void* request,
                                        ucs_status_t status,
                                        size_t length,
                                        void* user_data);
};
