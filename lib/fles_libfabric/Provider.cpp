// Copyright 2016 Thorsten Schuett <schuett@zib.de>

#include "Provider.hpp"
#include "VerbsProvider.hpp"
#include "MsgSocketsProvider.hpp"
#include "RDMSocketsProvider.hpp"
#include "GNIProvider.hpp"

#include <cassert>
#include <cstring>
#include <memory>
#include <iostream>

#include <rdma/fi_domain.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_endpoint.h>

#include "LibfabricException.hpp"

std::unique_ptr<Provider> Provider::get_provider()
{
    // std::cout << "Provider::get_provider()" << std::endl;
    struct fi_info* info = VerbsProvider::exists();
    if (info != nullptr)
        return std::unique_ptr<Provider>(new VerbsProvider(info));

    info = RDMSocketsProvider::exists();
    if (info != nullptr) {
        std::cout << "found rdm" << std::endl;
        return std::unique_ptr<Provider>(new RDMSocketsProvider(info));
    }

    info = MsgSocketsProvider::exists();
    if (info != nullptr)
        return std::unique_ptr<Provider>(new MsgSocketsProvider(info));

    info = GNIProvider::exists();
    if (info != nullptr)
        return std::unique_ptr<Provider>(new GNIProvider(info));

    throw LibfabricException("no known Libfabric provider found");
}

uint64_t Provider::requested_key = 0;

int Provider::vector = 0;
