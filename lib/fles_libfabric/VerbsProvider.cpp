// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "VerbsProvider.hpp"

#include <unistd.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>

#include "LibfabricException.hpp"

namespace tl_libfabric {

VerbsProvider::~VerbsProvider() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  fi_freeinfo(info_);
  fi_close((fid_t)fabric_);
#pragma GCC diagnostic pop
}

struct fi_info* VerbsProvider::exists(const std::string& local_host_name) {
  struct fi_info* hints = fi_allocinfo();
  struct fi_info* info = nullptr;

  hints->caps =
      FI_MSG | FI_RMA | FI_WRITE | FI_SEND | FI_RECV | FI_REMOTE_WRITE;
  hints->mode = FI_LOCAL_MR;
  hints->ep_attr->type = FI_EP_MSG;
  hints->rx_attr->mode = FI_LOCAL_MR | FI_RX_CQ_DATA;
  hints->domain_attr->threading = FI_THREAD_SAFE;
  hints->addr_format = FI_SOCKADDR_IN;
  hints->fabric_attr->prov_name = strdup("verbs");

  int res = fi_getinfo(FI_VERSION(1, 1), local_host_name.c_str(), nullptr,
                       FI_SOURCE, hints, &info);

  if (res == 0) {
    // fi_freeinfo(hints);
    return info;
  }

  fi_freeinfo(info);
  // fi_freeinfo(hints);

  return nullptr;
}

VerbsProvider::VerbsProvider(struct fi_info* info) : info_(info) {
  int res = fi_fabric(info_->fabric_attr, &fabric_, nullptr);
  if (res != 0) {
    L_(fatal) << "fi_fabric failed: " << res << "=" << fi_strerror(-res);
    throw LibfabricException("fi_fabric failed");
  }
}

void VerbsProvider::accept(struct fid_pep* pep,
                           const std::string& hostname,
                           unsigned short port,
                           unsigned int count,
                           fid_eq* eq) {
  unsigned int count_ = count;
  std::string port_s = std::to_string(port);

  // @todo find local ib device

  struct fi_info* accept_info = nullptr;
  int res = fi_getinfo(FI_VERSION(1, 1), hostname.c_str(), port_s.c_str(),
                       FI_SOURCE, info_, &accept_info);
  if (res != 0) {
    L_(fatal) << "lookup " << hostname << " in accept failed: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("lookup " + hostname + " in accept failed");
  }

  res = fi_passive_ep(fabric_, accept_info, &pep, nullptr);
  if (res != 0) {
    L_(fatal) << "fi_passive_ep in accept failed: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("fi_passive_ep in accept failed");
  }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  res = fi_control((fid_t)pep, FI_BACKLOG, &count_);
  if (res != 0) {
    L_(fatal) << "fi_control in accept failed: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("fi_control in accept failed");
  }
  assert(eq != nullptr);
  res = fi_pep_bind(pep, &eq->fid, 0);
  if (res != 0) {
    L_(fatal) << "fi_pep_bind in accept failed: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("fi_pep_bind in accept failed");
  }
#pragma GCC diagnostic pop
  res = fi_listen(pep);
  if (res != 0) {
    L_(fatal) << "fi_listen in accept failed: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("fi_listen in accept failed");
  }
}

void VerbsProvider::connect(::fid_ep* ep,
                            uint32_t /*max_send_wr*/,
                            uint32_t /*max_send_sge*/,
                            uint32_t /*max_recv_wr*/,
                            uint32_t /*max_recv_sge*/,
                            uint32_t /*max_inline_data*/,
                            const void* param,
                            size_t param_len,
                            void* /*addr*/) {
  int res = fi_connect(ep, nullptr, param, param_len);
  if (res != 0) {
    L_(fatal) << "fi_connect failed: " << res << "=" << fi_strerror(-res);
    throw LibfabricException("fi_connect failed");
  }
}
} // namespace tl_libfabric
