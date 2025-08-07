// Copyright 2025 Jan de Cuveland <cmail@cuveland.de>

#include "UcxConnection.hpp"
#include "log.hpp"
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <ucp/api/ucp.h>
#include <ucs/type/status.h>

UcxConnection::UcxConnection(UcpContext& context) : context_(context) {}

UcxConnection::~UcxConnection() { disconnect(); }

void UcxConnection::connect(const std::string& hostname, uint16_t port) {

  struct addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str = std::to_string(port);

  struct addrinfo* result;
  int err = getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &result);
  if (err != 0) {
    throw UcxException("Failed to resolve hostname: " +
                       std::string(gai_strerror(err)));
  }

  // Create endpoint to remote worker
  ucs_status_t status;
  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    ucp_ep_params_t ep_params{};
    ep_params.flags = UCP_EP_PARAMS_FLAGS_CLIENT_SERVER;
    ep_params.field_mask =
        UCP_EP_PARAM_FIELD_SOCK_ADDR | UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
        UCP_EP_PARAM_FIELD_ERR_HANDLER | UCP_EP_PARAM_FIELD_FLAGS;
    ep_params.sockaddr.addr = rp->ai_addr;
    ep_params.sockaddr.addrlen = rp->ai_addrlen;
    ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
    ep_params.err_handler.cb = err_handler_cb;
    ep_params.err_handler.arg = this; // Pass this instance to the callback

    status = ucp_ep_create(context_.worker(), &ep_params, &ep_);
    if (status != UCS_OK) {
      L_(error) << "(Loop) Failed to create UCX endpoint: "
                << ucs_status_string(status);
    } else {
      L_(debug) << "(Loop) UCX endpoint created successfully";
      break; // Exit loop on success
    }
  }
  freeaddrinfo(result);

  if (status != UCS_OK) {
    L_(error) << "Failed to create UCX endpoint: " << ucs_status_string(status);
    return;
  }
  connected_ = true;
  L_(info) << "Connected to remote endpoint" << std::endl;
}

void UcxConnection::disconnect() {
  L_(debug) << "Disconnecting from remote endpoint";

  if (ep_ == nullptr || disconnecting_) {
    return;
  }

  disconnecting_ = true;

  // Request endpoint close
  L_(debug) << "Requesting endpoint close";
  ucp_request_param_t param{};
  ucs_status_ptr_t request = ucp_ep_close_nbx(ep_, &param);

  if (request != nullptr && UCS_PTR_IS_PTR(request)) {
    ucs_status_t status;
    do {
      context_.progress();
      status = ucp_request_check_status(request);
    } while (status == UCS_INPROGRESS);

    ucp_request_free(request);
  }

  ep_ = nullptr;
  connected_ = false;
  disconnecting_ = false;

  L_(debug) << "Disconnected from remote endpoint";
}

void UcxConnection::progress() { context_.progress(); }

/// Send data with active message semantics
void UcxConnection::send_am(const void* buffer, size_t length) {
  ucp_request_param_t param{};
  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS | UCP_OP_ATTR_FIELD_CALLBACK |
                       UCP_OP_ATTR_FIELD_USER_DATA;
  param.flags = UCP_AM_SEND_FLAG_COPY_HEADER | UCP_AM_SEND_FLAG_RNDV;
  param.cb.send = send_am_cb;
  param.user_data = this;

  uint64_t header = 5678; // Example header, adjust as needed
  ucs_status_ptr_t request = ucp_am_send_nbx(ep_, 1234, &header, sizeof(header),
                                             buffer, length, &param);

  if (UCS_PTR_IS_ERR(request)) {
    L_(error) << "Failed to send active message: "
              << ucs_status_string(UCS_PTR_STATUS(request));
    return;
  };
  if (request == nullptr) {
    // Operation completed immediately
    L_(debug) << "Active message sent successfully";
  }
}

void UcxConnection::err_handler_cb(void* arg,
                                   ucp_ep_h ep,
                                   ucs_status_t status) {
  auto* connection = static_cast<UcxConnection*>(arg);
  L_(error) << "Error on UCX endpoint: " << ucs_status_string(status);

  // Handle disconnection
  if (connection->ep_ == ep) {
    connection->disconnect();
  }
}

void UcxConnection::send_am_cb(void* request,
                               ucs_status_t status,
                               void* /* user_data */) {
  // auto* connection = static_cast<UcxConnection*>(user_data);
  L_(info) << "AM Send Callback: " << ucs_status_string(status);
  ucp_request_free(request);
}