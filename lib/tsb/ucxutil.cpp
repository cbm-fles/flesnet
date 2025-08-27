// Copyright 2025 Jan de Cuveland

#include "ucxutil.hpp"
#include "log.hpp"
#include <charconv>
#include <netdb.h>
#include <sys/epoll.h>
#include <ucp/api/ucp.h>
#include <ucs/type/status.h>

namespace ucx::util {
bool init(ucp_context_h& context, ucp_worker_h& worker, int epoll_fd) {
  if (context != nullptr || worker != nullptr) {
    ERROR("UCP context or worker already initialized");
    return false;
  }

  // Initialize UCP context
  ucp_config_t* config = nullptr;
  ucs_status_t status = ucp_config_read(nullptr, nullptr, &config);
  if (status != UCS_OK) {
    ERROR("Failed to read UCP config");
    return false;
  }

  ucp_params_t ucp_params = {};
  // Request Active Message support
  ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
  ucp_params.features = UCP_FEATURE_AM | UCP_FEATURE_WAKEUP;

  status = ucp_init(&ucp_params, config, &context);
  ucp_config_release(config);
  if (status != UCS_OK) {
    ERROR("Failed to initialize UCP context");
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
    ERROR("Failed to create UCP worker");
    ucp_cleanup(context);
    context = nullptr;
    return false;
  }

  // Set up epoll
  int event_fd = -1;
  status = ucp_worker_get_efd(worker, &event_fd);
  if (status != UCS_OK) {
    ERROR("Failed to get UCP worker event_fd: {}", status);
    ucp_worker_destroy(worker);
    worker = nullptr;
    ucp_cleanup(context);
    context = nullptr;
    return false;
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = event_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &ev) == -1) {
    ERROR("Failed to set up epoll for UCP worker");
    ucp_worker_destroy(worker);
    worker = nullptr;
    ucp_cleanup(context);
    context = nullptr;
    return false;
  }

  DEBUG("UCP context and worker initialized successfully");
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

bool arm_worker_and_wait(ucp_worker_h worker, int epoll_fd, int timeout_ms) {
  ucs_status_t status = ucp_worker_arm(worker);
  if (status == UCS_ERR_BUSY) {
    return true;
  }
  if (status != UCS_OK) {
    ERROR("Failed to arm UCP worker: {}", status);
    return false;
  }

  std::array<epoll_event, 1> events{};
  int nfds = epoll_wait(epoll_fd, events.data(), events.size(), timeout_ms);
  if (nfds == -1) {
    if (errno == EINTR) {
      return true;
    }
    ERROR("epoll_wait failed: {}", strerror(errno));
    return false;
  }
  return true;
}

std::optional<std::string> get_client_address(ucp_conn_request_h conn_request) {
  ucp_conn_request_attr_t attr{};
  attr.field_mask = UCP_CONN_REQUEST_ATTR_FIELD_CLIENT_ADDR;

  ucs_status_t status = ucp_conn_request_query(conn_request, &attr);
  if (status != UCS_OK) {
    ERROR("Failed to query connection request for client address");
    return std::nullopt;
  }

  char host[NI_MAXHOST];
  char service[NI_MAXSERV];
  if (getnameinfo(
          reinterpret_cast<const struct sockaddr*>(&attr.client_address),
          sizeof(attr.client_address), static_cast<char*>(host), sizeof(host),
          static_cast<char*>(service), sizeof(service),
          NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
    ERROR("Failed to get client address from connection request: {}",
          gai_strerror(errno));
    return std::nullopt;
  }

  return std::string(static_cast<char*>(host)) + ":" +
         std::string(static_cast<char*>(service));
}

bool create_listener(ucp_worker_h worker,
                     ucp_listener_h& listener,
                     uint16_t listen_port,
                     ucp_listener_conn_callback_t conn_cb,
                     void* arg) {
  if (listener != nullptr) {
    return false;
  }

  struct sockaddr_in listen_addr {};
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  listen_addr.sin_port = htons(listen_port);

  ucp_listener_params_t params{};
  params.field_mask = UCP_LISTENER_PARAM_FIELD_SOCK_ADDR |
                      UCP_LISTENER_PARAM_FIELD_CONN_HANDLER;
  params.sockaddr.addr = reinterpret_cast<const struct sockaddr*>(&listen_addr);
  params.sockaddr.addrlen = sizeof(listen_addr);
  params.conn_handler.cb = conn_cb;
  params.conn_handler.arg = arg;

  ucs_status_t status = ucp_listener_create(worker, &params, &listener);
  if (status != UCS_OK) {
    ERROR("UCP listener creation failed: {}", status);
    return false;
  }
  INFO("Listening for connections on port {}", listen_port);

  return true;
}

std::optional<ucp_ep_h> create_endpoint(ucp_worker_h worker,
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
    ERROR("Failed to create endpoint from connection request");
    return std::nullopt;
  }

  return ep;
}

std::optional<ucp_ep_h> accept(ucp_worker_h worker,
                               ucp_conn_request_h conn_request,
                               ucp_err_handler_cb_t on_endpoint_error,
                               void* arg) {
  auto ep = create_endpoint(worker, conn_request, on_endpoint_error, arg);
  if (!ep) {
    ERROR("Failed to create endpoint for new connection");
    return std::nullopt;
  }
  return *ep;
}

std::optional<ucp_ep_h> connect(ucp_worker_h worker,
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
    ERROR("Failed to resolve hostname: {}", gai_strerror(err));
    return std::nullopt;
  }

  ucp_ep_h ep = nullptr;
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
      TRACE("UCX endpoint created successfully");
      break;
    }
    TRACE("Failed to create UCX endpoint: {}", status);
  }
  freeaddrinfo(result);

  if (ep == nullptr) {
    ERROR("Failed to connect to {}:{}", address, port);
    return std::nullopt;
  }
  DEBUG("Connecting to {}:{}", address, port);
  return ep;
}

void close_endpoint(ucp_worker_h worker, ucp_ep_h ep, bool force) {
  ucp_request_param_t param{};
  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
  param.flags = force ? UCP_EP_CLOSE_MODE_FORCE : UCP_EP_CLOSE_MODE_FLUSH;

  ucs_status_ptr_t request = ucp_ep_close_nbx(ep, &param);
  wait_for_request_completion(worker, request);
}

void wait_for_request_completion(ucp_worker_h worker,
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

bool set_receive_handler(ucp_worker_h worker,
                         unsigned int id,
                         ucp_am_recv_callback_t callback,
                         void* arg) {
  ucp_am_handler_param_t param{};
  param.field_mask = UCP_AM_HANDLER_PARAM_FIELD_ID |
                     UCP_AM_HANDLER_PARAM_FIELD_CB |
                     UCP_AM_HANDLER_PARAM_FIELD_ARG;
  param.id = id;
  param.cb = callback;
  param.arg = arg;

  ucs_status_t status = ucp_worker_set_am_recv_handler(worker, &param);
  if (status != UCS_OK) {
    ERROR("Failed to set active message receive handler: {}", status);
    return false;
  }
  DEBUG("Active message receive handler set for ID {}", id);
  return true;
}

bool send_active_message_with_params(ucp_ep_h ep,
                                     unsigned id,
                                     std::span<const std::byte> header,
                                     std::span<const std::byte> buffer,
                                     ucp_request_param_t& param) {

  ucs_status_ptr_t request =
      ucp_am_send_nbx(ep, id, header.data(), header.size(), buffer.data(),
                      buffer.size(), &param);

  if (UCS_PTR_IS_ERR(request)) {
    ucs_status_t status = UCS_PTR_STATUS(request);
    ERROR("Failed to send active message: {}", status);
    return false;
  }

  if (request == nullptr) {
    // Operation has completed successfully in-place
    TRACE("Active message sent successfully");
    if (param.cb.send != nullptr) {
      param.cb.send(nullptr, UCS_OK, param.user_data);
    }
  }

  return true;
}

bool send_active_message(ucp_ep_h ep,
                         unsigned id,
                         std::span<const std::byte> header,
                         std::span<const std::byte> buffer,
                         ucp_send_nbx_callback_t callback,
                         void* user_data,
                         uint32_t flags) {
  ucp_request_param_t param{};
  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS | UCP_OP_ATTR_FIELD_CALLBACK |
                       UCP_OP_ATTR_FIELD_USER_DATA;
  param.flags = flags;
  param.cb.send = callback;
  param.user_data = user_data;

  return send_active_message_with_params(ep, id, header, buffer, param);
}

void on_generic_send_complete(void* request,
                              ucs_status_t status,
                              [[maybe_unused]] void* user_data) {
  if (UCS_PTR_IS_ERR(request)) {
    ERROR("Send operation failed: {}", status);
  } else if (status != UCS_OK) {
    ERROR("Send operation completed with status: {}", status);
  } else {
    TRACE("Send operation completed successfully");
  }

  if (request != nullptr) {
    ucp_request_free(request);
  }
}

std::pair<std::string, uint16_t> parse_address(std::string_view address,
                                               uint16_t default_port) {
  auto last_colon = address.rfind(':');
  if (last_colon != std::string::npos) {
    std::string_view port_str = address.substr(last_colon + 1);
    unsigned long port_val;
    auto [ptr, ec] = std::from_chars(
        port_str.data(), port_str.data() + port_str.size(), port_val);
    if (ec == std::errc() && ptr == port_str.data() + port_str.size() &&
        port_val <= 65535) {
      return {std::string(address.substr(0, last_colon)),
              static_cast<uint16_t>(port_val)};
    }
  }
  return {std::string(address), default_port};
}

} // namespace ucx::util
