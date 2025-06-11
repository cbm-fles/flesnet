// Copyright 2025 Jan de Cuveland <cmail@cuveland.de>

#include "UcxListener.hpp"
#include "log.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ucp/api/ucp.h>
#include <ucs/type/status.h>

UcpListener::UcpListener(UcpContext& context,
                         uint16_t port,
                         void* recv_buffer_data,
                         size_t recv_buffer_size)
    : context_(context), recv_buffer_data_(recv_buffer_data),
      recv_buffer_size_(recv_buffer_size) {
  struct sockaddr_in listen_addr {};
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  listen_addr.sin_port = htons(port);

  ucp_listener_params_t params{};
  params.field_mask = UCP_LISTENER_PARAM_FIELD_SOCK_ADDR |
                      UCP_LISTENER_PARAM_FIELD_CONN_HANDLER;
  params.sockaddr.addr = reinterpret_cast<const struct sockaddr*>(&listen_addr);
  params.sockaddr.addrlen = sizeof(listen_addr);
  params.conn_handler.cb = static_conn_callback;
  params.conn_handler.arg = this;

  ucs_status_t status =
      ucp_listener_create(context.worker(), &params, &listener_);
  if (status != UCS_OK) {
    throw UcxException("Failed to create UCX listener");
  }

  L_(info) << "UCX listener created on port " << port;
}

UcpListener::~UcpListener() {
  if (listener_ != nullptr) {
    ucp_listener_destroy(listener_);
  }
}

void UcpListener::conn_callback(ucp_conn_request_h conn_request) {
  L_(info) << "New connection request received";

  // Get client address
  ucp_conn_request_attr_t attr{};
  attr.field_mask = UCP_CONN_REQUEST_ATTR_FIELD_CLIENT_ADDR;

  ucs_status_t status = ucp_conn_request_query(conn_request, &attr);
  if (status != UCS_OK) {
    L_(error) << "Failed to query connection request";
    ucp_listener_reject(listener_, conn_request);
    return;
  }

  ucp_ep_h ep;
  ucp_ep_params_t ep_params;
  memset(&ep_params, 0, sizeof(ep_params));

  ep_params.field_mask = UCP_EP_PARAM_FIELD_CONN_REQUEST |
                         UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
                         UCP_EP_PARAM_FIELD_ERR_HANDLER;
  ep_params.conn_request = conn_request;
  ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
  ep_params.err_handler.cb = err_handler_cb;
  ep_params.err_handler.arg = this; // Pass this instance to the callback

  status = ucp_ep_create(context_.worker(), &ep_params, &ep);
  if (status != UCS_OK) {
    L_(error) << "Failed to create endpoint from connection request";
    return;
  }

  std::cout << "Accepted connection from "
            << inet_ntoa(
                   reinterpret_cast<const sockaddr_in*>(&attr.client_address)
                       ->sin_addr)
            << std::endl;

  connections_.push_back(ep);

  ucp_am_handler_param_t param{};
  param.field_mask = UCP_AM_HANDLER_PARAM_FIELD_ID |
                     UCP_AM_HANDLER_PARAM_FIELD_CB |
                     UCP_AM_HANDLER_PARAM_FIELD_ARG;
  param.id = 1234; // Example AM ID, adjust as needed
  param.cb = am_recv_callback;
  param.arg = this; // Pass this instance to the callback
  status = ucp_worker_set_am_recv_handler(context_.worker(), &param);
  if (status != UCS_OK) {
    L_(error) << "Failed to set active message receive handler: "
              << ucs_status_string(status);
    // TODO: Use ucp_ep_close_nbx with FORCE instead
    ucp_ep_close_nb(ep, UCP_EP_CLOSE_MODE_FLUSH);
    return;
  }
  L_(info) << "Active message receive handler set for endpoint";
}

void UcpListener::static_conn_callback(ucp_conn_request_h conn_request,
                                       void* arg) {
  std::cout << "Static connection callback invoked" << std::endl;
  static_cast<UcpListener*>(arg)->conn_callback(conn_request);
}

void UcpListener::err_handler_cb(void* arg, ucp_ep_h ep, ucs_status_t status) {
  auto* connection = static_cast<UcpListener*>(arg);
  L_(error) << "Error on UCX endpoint: " << ucs_status_string(status);

  // Handle disconnection
  /*
  if (connection->ep_ == ep) {
    connection->disconnect();
  }
    */
}

ucs_status_t UcpListener::am_recv_callback(void* arg,
                                           const void* header,
                                           size_t header_length,
                                           void* data,
                                           size_t length,
                                           const ucp_am_recv_param_t* param) {
  auto* listener = static_cast<UcpListener*>(arg);
  L_(info) << "Received active message with header length " << header_length
           << " and data length " << length;
  // Process the received data
  if (header_length > 0) {
    uint64_t msg_id = *static_cast<const uint64_t*>(header);
    L_(info) << "Received message ID: " << msg_id;
  }
  if ((param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) != 0u) {
    L_(info) << "Received data with RNDV flag";
  } else {
    L_(error) << "ERROR: Received data without RNDV flag";
    return UCS_OK;
  }
  ucp_request_param_t req_param{};
  req_param.op_attr_mask =
      UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
  req_param.cb.recv_am = UcpListener::am_recv_data_nbx_callback;
  req_param.user_data = arg;

  ucs_status_ptr_t request = ucp_am_recv_data_nbx(
      listener->context_.worker(), data, listener->recv_buffer_data_,
      listener->recv_buffer_size_, &req_param);
  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Failed to receive active message data: "
              << ucs_status_string(UCS_PTR_STATUS(request));
    return UCS_ERR_IO_ERROR;
  }
  if (request == nullptr) {
    L_(debug) << "Active message data received successfully";
  } else {
    L_(debug) << "Active message data receive in progress";
  }
  return UCS_OK;
};

void UcpListener::am_recv_data_nbx_callback(void* request,
                                            ucs_status_t status,
                                            size_t length,
                                            void* user_data) {
  auto* listener = static_cast<UcpListener*>(user_data);
  L_(info) << "AM Recv Callback: " << ucs_status_string(status);
  // Print number of bytes received
  if (status == UCS_OK) {
    L_(info) << "Received " << length << " bytes of data";
    // Process the received data in listener->recv_buffer_data_
    // For example, you can print the first few bytes
    uint8_t* data = static_cast<uint8_t*>(listener->recv_buffer_data_);
    for (size_t i = 0; i < 100; ++i) {
      std::cout << std::hex << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
  } else {
    L_(error) << "Failed to receive data: " << ucs_status_string(status);
  }
  // Free the request if it was allocated
  if (request != nullptr && UCS_PTR_IS_PTR(request)) {
    ucp_request_free(request);
  }
}