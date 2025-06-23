// Copyright 2025 Jan de Cuveland

#include "ucxutil.hpp"
#include "log.hpp"
#include <stdexcept>
#include <ucp/api/ucp.h>
#include <ucs/type/status.h>

namespace ucx::util {
void ucx_init(ucp_context_h& context, ucp_worker_h& worker) {
  if (context != nullptr || worker != nullptr) {
    throw std::runtime_error("UCP context or worker already initialized");
  }

  // Initialize UCP context
  ucp_config_t* config = nullptr;
  ucs_status_t status = ucp_config_read(nullptr, nullptr, &config);
  if (status != UCS_OK) {
    throw std::runtime_error("Failed to read UCP config");
  }

  ucp_params_t ucp_params = {};
  // Request Active Message support
  ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
  ucp_params.features = UCP_FEATURE_AM | UCP_FEATURE_WAKEUP;

  status = ucp_init(&ucp_params, config, &context);
  ucp_config_release(config);

  if (status != UCS_OK) {
    throw std::runtime_error("Failed to initialize UCP context");
  }

  // Create UCP worker
  ucp_worker_params_t worker_params = {};
  // Only the master thread can access (i.e. the thread that initialized the
  // context; multiple threads may exist and never access)
  worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
  worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;

  status = ucp_worker_create(context, &worker_params, &worker);
  if (status != UCS_OK) {
    ucp_cleanup(context);
    throw std::runtime_error("Failed to create UCP worker");
  }
}

void ucx_cleanup(ucp_context_h& context, ucp_worker_h& worker) {
  if (worker != nullptr) {
    ucp_worker_destroy(worker);
    worker = nullptr;
  }
  if (context != nullptr) {
    ucp_cleanup(context);
    context = nullptr;
  }
}
} // namespace ucx::util