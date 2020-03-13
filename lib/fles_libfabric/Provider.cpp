// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#include "Provider.hpp"
#include "GNIProvider.hpp"
#include "MsgSocketsProvider.hpp"
#include "RDMSocketsProvider.hpp"
#include "VerbsProvider.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>

#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>

#include "LibfabricException.hpp"

namespace tl_libfabric {

std::unique_ptr<Provider>
Provider::get_provider(const std::string& local_host_name) {
  // std::cout << "Provider::get_provider()" << std::endl;
  struct fi_info* info = VerbsProvider::exists(local_host_name);
  if (info != nullptr) {
    std::cout << "found Verbs" << std::endl;
    return std::unique_ptr<Provider>(new VerbsProvider(info));
  }

  info = GNIProvider::exists(local_host_name);
  if (info != nullptr) {
    std::cout << "found GNI" << std::endl;
    return std::unique_ptr<Provider>(new GNIProvider(info));
  }

  info = MsgSocketsProvider::exists(local_host_name);
  if (info != nullptr) {
    std::cout << "found Sockets" << std::endl;
    return std::unique_ptr<Provider>(new MsgSocketsProvider(info));
  }

  info = RDMSocketsProvider::exists(local_host_name);
  if (info != nullptr) {
    std::cout << "found rdm" << std::endl;
    return std::unique_ptr<Provider>(new RDMSocketsProvider(info));
  }

  throw LibfabricException("no known Libfabric provider found");
}

uint64_t Provider::requested_key = 0;

std::unique_ptr<Provider> Provider::prov;

int Provider::vector = 0;
} // namespace tl_libfabric
