// Copyright 2025 Jan de Cuveland
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <ucp/api/ucp.h>

namespace ucx::util {
static constexpr int EPOLL_TIMEOUT_MS = 1000;
bool init(ucp_context_h& context, ucp_worker_h& worker, int epoll_fd);
void cleanup(ucp_context_h& context, ucp_worker_h& worker);
bool arm_worker_and_wait(ucp_worker_h worker,
                         int epoll_fd,
                         int timeout_ms = EPOLL_TIMEOUT_MS);
std::optional<std::string> get_client_address(ucp_conn_request_h conn_request);
bool create_listener(ucp_worker_h worker,
                     ucp_listener_h& listener,
                     uint16_t listen_port,
                     ucp_listener_conn_callback_t conn_cb,
                     void* arg);
std::optional<ucp_ep_h> accept(ucp_worker_h worker,
                               ucp_conn_request_h conn_request,
                               ucp_err_handler_cb_t on_endpoint_error,
                               void* arg);
std::optional<ucp_ep_h> connect(ucp_worker_h worker,
                                const std::string& address,
                                uint16_t port,
                                ucp_err_handler_cb_t on_endpoint_error,
                                void* arg);
void close_endpoint(ucp_worker_h worker, ucp_ep_h ep, bool force);
void wait_for_request_completion(ucp_worker_h worker,
                                 ucs_status_ptr_t& request);
bool set_receive_handler(ucp_worker_h worker,
                         unsigned int id,
                         ucp_am_recv_callback_t callback,
                         void* arg);
bool send_active_message(ucp_ep_h ep,
                         unsigned id,
                         std::span<const std::byte> header,
                         std::span<const std::byte> buffer,
                         ucp_send_nbx_callback_t callback,
                         void* user_data,
                         uint32_t flags);
void handle_generic_send_complete(void* request, ucs_status_t status);
void on_generic_send_complete(void* request,
                              ucs_status_t status,
                              void* user_data);
std::pair<std::string, uint16_t> parse_address(std::string_view address,
                                               uint16_t default_port);
} // namespace ucx::util
