// Copyright 2025 Jan de Cuveland

#include "ucxutil.hpp"
#include "log.hpp"
#include <netdb.h>
#include <string_view>
#include <ucp/api/ucp.h>
#include <ucs/type/status.h>

namespace ucx::util {
bool init(ucp_context_h& context, ucp_worker_h& worker, int& event_fd) {
  if (context != nullptr || worker != nullptr) {
    L_(error) << "UCP context or worker already initialized";
    return false;
  }

  // Initialize UCP context
  ucp_config_t* config = nullptr;
  ucs_status_t status = ucp_config_read(nullptr, nullptr, &config);
  if (status != UCS_OK) {
    L_(error) << "Failed to read UCP config";
    return false;
  }

  ucp_params_t ucp_params = {};
  // Request Active Message support
  ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
  ucp_params.features = UCP_FEATURE_AM | UCP_FEATURE_WAKEUP;

  status = ucp_init(&ucp_params, config, &context);
  ucp_config_release(config);
  if (status != UCS_OK) {
    L_(error) << "Failed to initialize UCP context";
    return false;
  }

  // Create UCP worker
  ucp_worker_params_t worker_params = {};
  // Only the master thread can access (i.e. the thread that initialized the
  // context; multiple threads may exist and never access)
  worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
  worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;

  status = ucp_worker_create(context, &worker_params, &worker);
  if (status != UCS_OK) {
    L_(error) << "Failed to create UCP worker";
    ucp_cleanup(context);
    context = nullptr;
    return false;
  }

  status = ucp_worker_get_efd(worker, &event_fd);
  if (status != UCS_OK) {
    L_(error) << "Failed to get UCP worker event_fd: " +
                     std::string(ucs_status_string(status));
    ucp_worker_destroy(worker);
    worker = nullptr;
    ucp_cleanup(context);
    context = nullptr;
  }

  return true;
}

void cleanup(ucp_context_h& context, ucp_worker_h& worker) {
  if (worker != nullptr) {
    ucp_worker_destroy(worker);
    worker = nullptr;
  }
  if (context != nullptr) {
    ucp_cleanup(context);
    context = nullptr;
  }
}

std::optional<std::string> get_client_address(ucp_conn_request_h conn_request) {
  ucp_conn_request_attr_t attr{};
  attr.field_mask = UCP_CONN_REQUEST_ATTR_FIELD_CLIENT_ADDR;

  ucs_status_t status = ucp_conn_request_query(conn_request, &attr);
  if (status != UCS_OK) {
    L_(error) << "Failed to query connection request for client address";
    return std::nullopt;
  }

  char host[NI_MAXHOST];
  char service[NI_MAXSERV];
  if (getnameinfo(
          reinterpret_cast<const struct sockaddr*>(&attr.client_address),
          sizeof(attr.client_address), static_cast<char*>(host), sizeof(host),
          static_cast<char*>(service), sizeof(service),
          NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
    L_(error) << "Failed to get client address from connection request: "
              << gai_strerror(errno);
    return std::nullopt;
  }

  return std::string(static_cast<char*>(host)) + ":" +
         std::string(static_cast<char*>(service));
}

std::optional<ucp_ep_h> create_endpoint(ucp_worker_h& worker,
                                        ucp_conn_request_h conn_request,
                                        ucp_err_handler_cb_t on_endpoint_error,
                                        void* arg) {
  ucp_ep_h ep;
  ucp_ep_params_t ep_params{};
  ep_params.field_mask = UCP_EP_PARAM_FIELD_CONN_REQUEST |
                         UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
                         UCP_EP_PARAM_FIELD_ERR_HANDLER;
  ep_params.conn_request = conn_request;
  ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
  ep_params.err_handler.cb = on_endpoint_error;
  ep_params.err_handler.arg = arg;

  ucs_status_t status = ucp_ep_create(worker, &ep_params, &ep);
  if (status != UCS_OK) {
    L_(error) << "Failed to create endpoint from connection request";
    return std::nullopt;
  }

  return ep;
}

std::optional<ucp_ep_h> accept(ucp_worker_h& worker,
                               ucp_conn_request_h conn_request,
                               ucp_err_handler_cb_t on_endpoint_error,
                               void* arg) {
  auto ep = create_endpoint(worker, conn_request, on_endpoint_error, arg);
  if (!ep) {
    L_(error) << "Failed to create endpoint for new connection";
    return std::nullopt;
  }
  return *ep;
}

std::optional<ucp_ep_h> connect(ucp_worker_h& worker,
                                const std::string& address,
                                uint16_t port,
                                ucp_err_handler_cb_t on_endpoint_error,
                                void* arg) {
  struct addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str = std::to_string(port);
  struct addrinfo* result;
  int err = getaddrinfo(address.c_str(), port_str.c_str(), &hints, &result);
  if (err != 0) {
    L_(error) << "Failed to resolve hostname: " +
                     std::string(gai_strerror(err));
    return std::nullopt;
  }

  ucp_ep_h ep;
  ucp_ep_params_t ep_params{};
  ep_params.flags = UCP_EP_PARAMS_FLAGS_CLIENT_SERVER;
  ep_params.field_mask =
      UCP_EP_PARAM_FIELD_SOCK_ADDR | UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
      UCP_EP_PARAM_FIELD_ERR_HANDLER | UCP_EP_PARAM_FIELD_FLAGS;
  ep_params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
  ep_params.err_handler.cb = on_endpoint_error;
  ep_params.err_handler.arg = arg;

  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    ep_params.sockaddr.addr = rp->ai_addr;
    ep_params.sockaddr.addrlen = rp->ai_addrlen;

    ucs_status_t status = ucp_ep_create(worker, &ep_params, &ep);
    if (status == UCS_OK) {
      L_(debug) << "UCX endpoint created successfully";
      break;
    }
    L_(error) << "Failed to create UCX endpoint: " << ucs_status_string(status);
  }
  freeaddrinfo(result);
  if (ep == nullptr) {
    L_(error) << "Failed to create UCX endpoint";
    return std::nullopt;
  }
  L_(debug) << "Connected to scheduler at " << address << ":" << port;
  return ep;
}
// TODO: fix control flow!!!, return nullopt on error

void wait_for_request_completion(ucp_worker_h& worker,
                                 ucs_status_ptr_t& request) {
  if (request != nullptr && UCS_PTR_IS_PTR(request)) {
    ucs_status_t status;
    do {
      ucp_worker_progress(worker);
      status = ucp_request_check_status(request);
    } while (status == UCS_INPROGRESS);

    ucp_request_free(request);
  }
}

void close_endpoint(ucp_worker_h& worker, ucp_ep_h ep, bool force) {
  ucp_request_param_t param{};
  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
  param.flags = force ? UCP_EP_CLOSE_MODE_FORCE : UCP_EP_CLOSE_MODE_FLUSH;

  ucs_status_ptr_t request = ucp_ep_close_nbx(ep, &param);
  wait_for_request_completion(worker, request);
}

} // namespace ucx::util