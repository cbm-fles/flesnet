// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "MsgGNIProvider.hpp"

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

MsgGNIProvider::~MsgGNIProvider() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  fi_freeinfo(info_);
  fi_close((fid_t)fabric_);
#pragma GCC diagnostic pop
}

struct fi_info* MsgGNIProvider::exists(std::string local_host_name) {
  struct fi_info* hints =
      Provider::get_hints(FI_EP_MSG, "gni"); // fi_allocinfo();
  struct fi_info* info = nullptr;

  int res =
      fi_getinfo(FIVERSION, local_host_name.c_str(), nullptr, 0, hints, &info);

  if (res == 0) {
    // fi_freeinfo(hints);
    return info;
  }

  fi_freeinfo(info);
  // fi_freeinfo(hints);

  return nullptr;
}

MsgGNIProvider::MsgGNIProvider(struct fi_info* info) : info_(info) {
  int res = fi_fabric(info_->fabric_attr, &fabric_, nullptr);
  if (res != 0) {
    L_(fatal) << "fi_fabric failed: " << res << "=" << fi_strerror(-res);
    throw LibfabricException("fi_fabric failed");
  }
}

void MsgGNIProvider::accept(struct fid_pep* pep,
                            const std::string& hostname,
                            unsigned short port,
                            unsigned int /*count*/,
                            fid_eq* eq) {
  std::string port_s = std::to_string(port);
  struct fi_info* hints = Provider::get_hints(FI_EP_MSG, "gni");

  struct fi_info* accept_info = nullptr;
  int res = fi_getinfo(FIVERSION, hostname.c_str(), port_s.c_str(), FI_SOURCE,
                       hints, &accept_info);

  if (res != 0) {
    L_(fatal) << "lookup " << hostname << " in accept failed: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("lookup " + hostname + " in accept failed");
  }

  // inet_ntop(AF_INET, &(sa.sin_addr), str, INET_ADDRSTRLEN);

  // assert(accept_info->addr_format == FI_SOCKADDR_IN);

  res = fi_passive_ep(fabric_, accept_info, &pep, nullptr);
  if (res) {
    L_(fatal) << "fi_passive_ep in accept failed: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("fi_passive_ep in accept failed");
  }

  assert(eq != nullptr);
  res = fi_pep_bind(pep, &eq->fid, 0);
  if (res != 0) {
    L_(fatal) << "fi_pep_bind in accept failed: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("fi_pep_bind in accept failed");
  }
  res = fi_listen(pep);
  if (res != 0) {
    L_(fatal) << "fi_listen in accept failed: " << res << "="
              << fi_strerror(-res);
    throw LibfabricException("fi_listen in accept failed");
  }
}

void MsgGNIProvider::connect(fid_ep* ep,
                             uint32_t /*max_send_wr*/,
                             uint32_t /*max_send_sge*/,
                             uint32_t /*max_recv_wr*/,
                             uint32_t /*max_recv_sge*/,
                             uint32_t /*max_inline_data*/,
                             const void* param,
                             size_t param_len,
                             void* addr) {
  int res = fi_connect(ep, addr, param, param_len);
  if (res != 0) {
    L_(fatal) << "fi_connect failed: " << res << "=" << fi_strerror(-res);
    throw LibfabricException("fi_connect failed");
  }
}
} // namespace tl_libfabric
