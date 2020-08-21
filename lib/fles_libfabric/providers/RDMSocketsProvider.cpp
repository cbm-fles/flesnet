// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "RDMSocketsProvider.hpp"

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

#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>

namespace tl_libfabric {

RDMSocketsProvider::~RDMSocketsProvider() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  fi_freeinfo(info_);
  fi_close((::fid_t)fabric_);
#pragma GCC diagnostic pop
}

struct fi_info* RDMSocketsProvider::exists(std::string local_host_name) {
  struct fi_info* hints =
      Provider::get_hints(FI_EP_RDM, "sockets"); // fi_allocinfo();
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

RDMSocketsProvider::RDMSocketsProvider(struct fi_info* info) : info_(info) {
  int res = fi_fabric(info_->fabric_attr, &fabric_, nullptr);
  if (res != 0) {
    L_(fatal) << "fi_fabric failed: " << res << "=" << fi_strerror(-res);
    throw LibfabricException("fi_fabric failed");
  }
}

void RDMSocketsProvider::accept(struct fid_pep* pep __attribute__((unused)),
                                const std::string& hostname
                                __attribute__((unused)),
                                unsigned short port __attribute__((unused)),
                                unsigned int count __attribute__((unused)),
                                fid_eq* eq __attribute__((unused))) {
  // there is no accept for RDM
}

void RDMSocketsProvider::connect(::fid_ep* ep __attribute__((unused)),
                                 uint32_t max_send_wr __attribute__((unused)),
                                 uint32_t max_send_sge __attribute__((unused)),
                                 uint32_t max_recv_wr __attribute__((unused)),
                                 uint32_t max_recv_sge __attribute__((unused)),
                                 uint32_t max_inline_data
                                 __attribute__((unused)),
                                 const void* param __attribute__((unused)),
                                 size_t param_len __attribute__((unused)),
                                 void* addr __attribute__((unused))) {
  // @todo send mr message?
}

void RDMSocketsProvider::set_hostnames_and_services(
    struct fid_av* av,
    const std::vector<std::string>& compute_hostnames,
    const std::vector<std::string>& compute_services,
    std::vector<::fi_addr_t>& fi_addrs) {
  struct fi_info *info, *hints;

  for (size_t i = 0; i < compute_hostnames.size(); i++) {
    fi_addr_t fi_addr;

    info = nullptr;
    hints = Provider::get_hints(FI_EP_RDM, "sockets"); // fi_allocinfo();

    int res = fi_getinfo(FIVERSION, compute_hostnames[i].c_str(),
                         compute_services[i].c_str(), 0, hints, &info);
    assert(res == 0);
    assert(info != NULL);
    assert(info->dest_addr != NULL);
    res = fi_av_insert(av, info->dest_addr, 1, &fi_addr, 0, NULL);
    assert(res == 1);
    fi_addrs.push_back(fi_addr);
    fi_freeinfo(info);
  }
}
} // namespace tl_libfabric
