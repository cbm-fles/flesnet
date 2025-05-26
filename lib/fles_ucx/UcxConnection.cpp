// Copyright 2023-2025 Jan de Cuveland <cmail@cuveland.de>

#include "UcxConnection.hpp"
#include "log.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Struct to hold data for callbacks
struct CallbackData {
  UcxConnection::CompletionCallback user_callback;
  void* buffer;
  size_t length;
};

UcxConnection::UcxConnection(UcxContext& context,
                             uint_fast16_t connection_index,
                             uint_fast16_t remote_connection_index)
    : context_(context), index_(connection_index),
      remote_index_(remote_connection_index) {}

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

  // Create socket and connect
  int sockfd = -1;
  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd == -1) {
      continue;
    }

    if (::connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
      break;
    }

    close(sockfd);
    sockfd = -1;
  }

  freeaddrinfo(result);

  if (sockfd == -1) {
    throw UcxException("Failed to connect to server");
  }

  // Exchange connection information
  ucp_address_t* local_addr;
  size_t local_addr_len;
  ucs_status_t status =
      ucp_worker_get_address(context_.worker(), &local_addr, &local_addr_len);
  if (status != UCS_OK) {
    close(sockfd);
    throw UcxException("Failed to get worker address");
  }

  // Send local address length and connection ID
  UcxConnectionInfo local_info{};
  local_info.address = local_addr;
  local_info.address_length = local_addr_len;
  local_info.connection_id = index_;

  ssize_t sent = send(sockfd, &local_info, sizeof(local_info), 0);
  if (sent != sizeof(local_info)) {
    ucp_worker_release_address(context_.worker(), local_addr);
    close(sockfd);
    throw UcxException("Failed to send connection info");
  }

  // Send address buffer
  sent = send(sockfd, local_addr, local_addr_len, 0);
  if (sent != static_cast<ssize_t>(local_addr_len)) {
    ucp_worker_release_address(context_.worker(), local_addr);
    close(sockfd);
    throw UcxException("Failed to send address");
  }

  ucp_worker_release_address(context_.worker(), local_addr);

  // Receive remote address info
  UcxConnectionInfo remote_info{};
  ssize_t received = recv(sockfd, &remote_info, sizeof(remote_info), 0);
  if (received != sizeof(remote_info)) {
    close(sockfd);
    throw UcxException("Failed to receive remote connection info");
  }

  // Allocate buffer for remote address
  std::vector<uint8_t> remote_addr_buf(remote_info.address_length);

  // Receive remote address
  received =
      recv(sockfd, remote_addr_buf.data(), remote_info.address_length, 0);
  if (received != static_cast<ssize_t>(remote_info.address_length)) {
    close(sockfd);
    throw UcxException("Failed to receive remote address");
  }

  close(sockfd);

  // Create endpoint to remote worker
  ucp_ep_params_t ep_params;
  memset(&ep_params, 0, sizeof(ep_params));

  ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
  ep_params.address = reinterpret_cast<ucp_address_t*>(remote_addr_buf.data());

  status = ucp_ep_create(context_.worker(), &ep_params, &ep_);
  if (status != UCS_OK) {
    throw UcxException("Failed to create endpoint");
  }

  connected_ = true;
  L_(debug) << "[" << index_ << "] Connected to remote endpoint "
            << remote_info.connection_id;
}

void UcxConnection::disconnect() {
  if (ep_ == nullptr || disconnecting_) {
    return;
  }

  disconnecting_ = true;

  // Request endpoint close
  void* request = ucp_ep_close_nb(ep_, UCP_EP_CLOSE_MODE_FLUSH);

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

  L_(debug) << "[" << index_ << "] Disconnected from remote endpoint";
}

void UcxConnection::progress() { context_.progress(); }

void UcxConnection::flush() {
  if (!connected_) {
    return;
  }

  void* request = ucp_ep_flush_nb(ep_, 0, nullptr);
  if (request != nullptr && UCS_PTR_IS_PTR(request)) {
    ucs_status_t status;
    do {
      context_.progress();
      status = ucp_request_check_status(request);
    } while (status == UCS_INPROGRESS);

    ucp_request_free(request);
  }
}

void UcxConnection::send_tagged(const void* buffer,
                                size_t length,
                                uint64_t tag,
                                CompletionCallback callback) {
  if (!connected_) {
    if (callback) {
      callback(UCS_ERR_NOT_CONNECTED);
    }
    return;
  }

  auto* cb_data = new CallbackData{callback, const_cast<void*>(buffer), length};

  ucp_request_param_t param;
  memset(&param, 0, sizeof(param)); // Zero initialize all fields first
  param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
  param.cb.send = send_callback; // Use the class method instead of lambda
  param.user_data = cb_data;

  void* request = ucp_tag_send_nbx(ep_, buffer, length, tag, &param);

  if (UCS_PTR_IS_ERR(request)) {
    ucs_status_t status = UCS_PTR_STATUS(request);
    if (callback) {
      callback(status);
    }
    delete cb_data;
  } else if (request == nullptr) {
    // Operation completed immediately
    if (callback) {
      callback(UCS_OK);
    }
    delete cb_data;
  }
  // For pending operations, the callback will be invoked later
  // and send_callback will handle the cleanup
}

void UcxConnection::recv_tagged(void* buffer,
                                size_t length,
                                uint64_t tag,
                                CompletionCallback callback) {
  if (!connected_) {
    if (callback) {
      callback(UCS_ERR_NOT_CONNECTED);
    }
    return;
  }

  auto* cb_data = new CallbackData{callback, buffer, length};

  ucp_request_param_t param;
  memset(&param, 0, sizeof(param)); // Zero initialize all fields first
  param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
  param.cb.recv = recv_callback; // Use the class method instead of lambda
  param.user_data = cb_data;

  void* request =
      ucp_tag_recv_nbx(context_.worker(), buffer, length, tag, 0, &param);

  if (UCS_PTR_IS_ERR(request)) {
    ucs_status_t status = UCS_PTR_STATUS(request);
    if (callback) {
      callback(status);
    }
    delete cb_data;
  } else if (request == nullptr) {
    // Operation completed immediately
    if (callback) {
      callback(UCS_OK);
    }
    delete cb_data;
  }
  // For pending operations, the callback will be invoked later
  // and recv_callback will handle the cleanup
}

void UcxConnection::rma_put(const void* buffer,
                            size_t length,
                            uint64_t remote_addr,
                            ucp_rkey_h rkey,
                            CompletionCallback callback) {
  if (!connected_) {
    if (callback) {
      callback(UCS_ERR_NOT_CONNECTED);
    }
    return;
  }

  auto* cb_data = new CallbackData{callback, const_cast<void*>(buffer), length};

  ucp_request_param_t param;
  param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
  param.cb.send = send_callback;
  param.user_data = cb_data;

  void* request = ucp_put_nbx(ep_, buffer, length, remote_addr, rkey, &param);

  if (UCS_PTR_IS_ERR(request)) {
    ucs_status_t status = UCS_PTR_STATUS(request);
    if (callback) {
      callback(status);
    }
    delete cb_data;
  } else if (request == nullptr) {
    // Operation completed immediately
    if (callback) {
      callback(UCS_OK);
    }
    delete cb_data;
  }
}

void UcxConnection::rma_get(void* buffer,
                            size_t length,
                            uint64_t remote_addr,
                            ucp_rkey_h rkey,
                            CompletionCallback callback) {
  if (!connected_) {
    if (callback) {
      callback(UCS_ERR_NOT_CONNECTED);
    }
    return;
  }

  auto* cb_data = new CallbackData{callback, buffer, length};

  ucp_request_param_t param;
  param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_USER_DATA;
  param.cb.send =
      send_callback; // Using send callback, as get uses same prototype
  param.user_data = cb_data;

  void* request = ucp_get_nbx(ep_, buffer, length, remote_addr, rkey, &param);

  if (UCS_PTR_IS_ERR(request)) {
    ucs_status_t status = UCS_PTR_STATUS(request);
    if (callback) {
      callback(status);
    }
    delete cb_data;
  } else if (request == nullptr) {
    // Operation completed immediately
    if (callback) {
      callback(UCS_OK);
    }
    delete cb_data;
  }
}

void UcxConnection::send_callback(void* request,
                                  ucs_status_t status,
                                  void* user_data) {
  auto* cb_data = static_cast<CallbackData*>(user_data);
  if (cb_data->user_callback) {
    cb_data->user_callback(status);
  }
  delete cb_data;
  if (request != nullptr) {
    ucp_request_free(request);
  }
}

void UcxConnection::recv_callback(void* request,
                                  ucs_status_t status,
                                  const ucp_tag_recv_info_t* /* info */,
                                  void* user_data) {
  auto* cb_data = static_cast<CallbackData*>(user_data);
  if (cb_data->user_callback) {
    cb_data->user_callback(status);
  }
  delete cb_data;
  if (request != nullptr) {
    ucp_request_free(request);
  }
}
