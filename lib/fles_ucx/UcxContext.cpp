// Copyright 2023-2025 Jan de Cuveland <cmail@cuveland.de>

#include "UcxContext.hpp"
#include "log.hpp"
#include <ucp/api/ucp.h>

UcxContext::UcxContext() {
  // Initialize UCX context
  ucp_config_t* config = nullptr;
  ucs_status_t status = ucp_config_read(nullptr, nullptr, &config);
  if (status != UCS_OK) {
    throw UcxException("Failed to read UCX config");
  }

  ucp_params_t ucp_params = {};
  ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
  ucp_params.features = UCP_FEATURE_RMA | UCP_FEATURE_TAG;

  status = ucp_init(&ucp_params, config, &ucp_context_);
  ucp_config_release(config);

  if (status != UCS_OK) {
    throw UcxException("Failed to initialize UCX");
  }

  // Create UCX worker
  ucp_worker_params_t worker_params = {};
  worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
  worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;

  status = ucp_worker_create(ucp_context_, &worker_params, &ucp_worker_);
  if (status != UCS_OK) {
    ucp_cleanup(ucp_context_);
    throw UcxException("Failed to create UCX worker");
  }

  L_(info) << "UCX context and worker initialized successfully";
}

UcxContext::~UcxContext() {
  if (ucp_worker_ != nullptr) {
    ucp_worker_destroy(ucp_worker_);
  }
  if (ucp_context_ != nullptr) {
    ucp_cleanup(ucp_context_);
  }
}

void UcxContext::progress() { ucp_worker_progress(ucp_worker_); }

void UcxContext::wait() { ucp_worker_wait(ucp_worker_); }

UcxMemoryRegion::UcxMemoryRegion(UcxContext& context,
                                 void* addr,
                                 size_t length,
                                 unsigned flags)
    : context_(context) {

  ucp_mem_map_params_t map_params = {};

  map_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                          UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                          UCP_MEM_MAP_PARAM_FIELD_FLAGS;
  map_params.address = addr;
  map_params.length = length;
  map_params.flags = flags;

  ucs_status_t status = ucp_mem_map(context.context(), &map_params, &memh_);
  if (status != UCS_OK) {
    throw UcxException("Failed to map memory region");
  }

  // Get remote key buffer for this memory region
  ucp_memh_pack_params_t pack_params = {};
  status = ucp_memh_pack(memh_, &pack_params, &rkey_buffer_, &rkey_length_);
  if (status != UCS_OK) {
    ucp_mem_unmap(context.context(), memh_);
    throw UcxException("Failed to pack memory handle");
  }
}

UcxMemoryRegion::~UcxMemoryRegion() {
  if (rkey_buffer_ != nullptr) {
    ucp_memh_buffer_release_params_t release_params = {};
    ucp_memh_buffer_release(rkey_buffer_, &release_params);
  }
  if (memh_ != nullptr) {
    ucp_mem_unmap(context_.context(), memh_);
  }
}
