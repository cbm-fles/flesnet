// Copyright 2025 Jan de Cuveland <cmail@cuveland.de>

#include "UcxContext.hpp"
#include "log.hpp"

UcpContext::UcpContext() {
  // Initialize UCP context
  ucp_config_t* config = nullptr;
  ucs_status_t status = ucp_config_read(nullptr, nullptr, &config);
  if (status != UCS_OK) {
    throw UcxException("Failed to read UCP config");
  }

  ucp_params_t ucp_params = {};
  // Request Active Message support
  ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
  ucp_params.features = UCP_FEATURE_AM;

  status = ucp_init(&ucp_params, config, &ucp_context_);
  ucp_config_release(config);

  if (status != UCS_OK) {
    throw UcxException("Failed to initialize UCP context");
  }

  // Create UCP worker
  ucp_worker_params_t worker_params = {};
  // Only the master thread can access (i.e. the thread that initialized the
  // context; multiple threads may exist and never access)
  worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
  worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;

  status = ucp_worker_create(ucp_context_, &worker_params, &ucp_worker_);
  if (status != UCS_OK) {
    ucp_cleanup(ucp_context_);
    throw UcxException("Failed to create UCP worker");
  }

  L_(info) << "UCP context and worker initialized successfully";
}

UcpContext::~UcpContext() {
  if (ucp_worker_ != nullptr) {
    ucp_worker_destroy(ucp_worker_);
  }
  if (ucp_context_ != nullptr) {
    ucp_cleanup(ucp_context_);
  }
}
