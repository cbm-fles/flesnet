// Copyright 2025 Jan de Cuveland
#pragma once

#include <ucp/api/ucp.h>

namespace ucx::util {
void ucx_init(ucp_context_h& context, ucp_worker_h& worker);
void ucx_cleanup(ucp_context_h& context, ucp_worker_h& worker);
} // namespace ucx::util
