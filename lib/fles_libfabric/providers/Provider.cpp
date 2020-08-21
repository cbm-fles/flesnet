// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "Provider.hpp"
#include "MsgGNIProvider.hpp"
#include "MsgSocketsProvider.hpp"
#include "MsgVerbsProvider.hpp"
#include "RDMGNIProvider.hpp"
#include "RDMOmniPathProvider.hpp"
#include "RDMSocketsProvider.hpp"
#include "RxMVerbsProvider.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>

#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>

#include "LibfabricException.hpp"

namespace tl_libfabric {

std::unique_ptr<Provider> Provider::get_provider(std::string local_host_name) {
  // std::cout << "Provider::get_provider()" << std::endl;
  struct fi_info* fiinfo = RDMOmniPathProvider::exists(local_host_name);
  if (fiinfo != nullptr) {
    L_(info) << "found OmniPath";
    return std::unique_ptr<Provider>(new RDMOmniPathProvider(fiinfo));
  }

  fiinfo = MsgVerbsProvider::exists(local_host_name);
  if (fiinfo != nullptr) {
    L_(info) << "found Verbs";
    return std::unique_ptr<Provider>(new MsgVerbsProvider(fiinfo));
  }

  fiinfo = RxMVerbsProvider::exists(local_host_name);
  if (fiinfo != nullptr) {
    L_(info) << "found RxM Verbs";
    return std::unique_ptr<Provider>(new RxMVerbsProvider(fiinfo));
  }

  fiinfo = MsgGNIProvider::exists(local_host_name);
  if (fiinfo != nullptr) {
    L_(info) << "found MSG GNI";
    return std::unique_ptr<Provider>(new MsgGNIProvider(fiinfo));
  }

  fiinfo = RDMGNIProvider::exists(local_host_name);
  if (fiinfo != nullptr) {
    L_(info) << "found RDM GNI";
    return std::unique_ptr<Provider>(new RDMGNIProvider(fiinfo));
  }

  fiinfo = MsgSocketsProvider::exists(local_host_name);
  if (fiinfo != nullptr) {
    L_(info) << "found Sockets";
    return std::unique_ptr<Provider>(new MsgSocketsProvider(fiinfo));
  }

  fiinfo = RDMSocketsProvider::exists(local_host_name);
  if (fiinfo != nullptr) {
    L_(info) << "found rdm";
    return std::unique_ptr<Provider>(new RDMSocketsProvider(fiinfo));
  }

  throw LibfabricException("no known Libfabric provider found");
}

struct fi_info* Provider::get_hints(enum fi_ep_type ep_type, std::string prov) {
  struct fi_info* hints = fi_allocinfo();

  hints->caps = FI_MSG | FI_RMA | FI_WRITE | FI_SEND | FI_RECV |
                FI_REMOTE_WRITE | FI_TAGGED;
  hints->mode = FI_LOCAL_MR | FI_CONTEXT;
  hints->ep_attr->type = ep_type;
  hints->domain_attr->threading = FI_THREAD_COMPLETION;
  hints->domain_attr->data_progress = FI_PROGRESS_AUTO;
  hints->domain_attr->mr_mode = FI_MR_BASIC;
  hints->fabric_attr->prov_name = strdup(prov.c_str());

  return hints;
}

uint64_t Provider::requested_key = 0;

std::unique_ptr<Provider> Provider::prov;

int Provider::vector = 0;
} // namespace tl_libfabric
