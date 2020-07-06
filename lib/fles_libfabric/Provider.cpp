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
  struct fi_info* info = RDMOmniPathProvider::exists(local_host_name);
  if (info != nullptr) {
    std::cout << "found OmniPath" << std::endl;
    return std::unique_ptr<Provider>(new RDMOmniPathProvider(info));
  }

  info = MsgVerbsProvider::exists(local_host_name);
  if (info != nullptr) {
    std::cout << "found Verbs" << std::endl;
    return std::unique_ptr<Provider>(new MsgVerbsProvider(info));
  }

  info = RxMVerbsProvider::exists(local_host_name);
  if (info != nullptr) {
    std::cout << "found RxM Verbs" << std::endl;
    return std::unique_ptr<Provider>(new RxMVerbsProvider(info));
  }

  info = MsgGNIProvider::exists(local_host_name);
  if (info != nullptr) {
    std::cout << "found MSG GNI" << std::endl;
    return std::unique_ptr<Provider>(new MsgGNIProvider(info));
  }

  info = RDMGNIProvider::exists(local_host_name);
  if (info != nullptr) {
    std::cout << "found RDM GNI" << std::endl;
    return std::unique_ptr<Provider>(new RDMGNIProvider(info));
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

void Provider::dump_fi_info(const struct fi_info* info) {
  fprintf(stdout, "\nSTART dumping a fi_info object ...\n");
  int count = 0;
  while (info != NULL) {
    fprintf(stdout, "---Dumpingfi_info[%d]---\n", count);
    fprintf(stdout, "\t caps %llu\n", info->caps);

    /*/////////////// tx_attr ////////////////////////////// */
    fprintf(stdout, "\t\t tx_attr->caps %llu\n", info->tx_attr->caps);
    fprintf(stdout, "\t\t tx_attr->comp_order %llu\n",
            info->tx_attr->comp_order);
    fprintf(stdout, "\t\t tx_attr->inject_size %zd\n",
            info->tx_attr->inject_size);
    fprintf(stdout, "\t\t tx_attr->iov_limit %zd\n", info->tx_attr->iov_limit);
    fprintf(stdout, "\t\t tx_attr->mode %llu\n", info->tx_attr->mode);
    fprintf(stdout, "\t\t tx_attr->msg_order %llu\n", info->tx_attr->msg_order);
    fprintf(stdout, "\t\t tx_attr->op_flags %llu\n", info->tx_attr->op_flags);
    fprintf(stdout, "\t\t tx_attr->rma_iov_limit %zd\n",
            info->tx_attr->rma_iov_limit);
    fprintf(stdout, "\t\t tx_attr->size %zd\n", info->tx_attr->size);
    /*/////////////// END tx_attr /////////////////////////////// */

    fprintf(stdout, "\t mode %llu\n", info->mode);

    /*/////////////// rx_attr ////////////////////////////// */
    fprintf(stdout, "\t\t rx_attr->caps %llu\n", info->rx_attr->caps);
    fprintf(stdout, "\t\t rx_attr->comp_order %llu\n",
            info->rx_attr->comp_order);
    fprintf(stdout, "\t\t rx_attr->iov_limit %zd\n", info->rx_attr->iov_limit);
    fprintf(stdout, "\t\t rx_attr->mode %llu\n", info->rx_attr->mode);
    fprintf(stdout, "\t\t rx_attr->msg_order %llu\n", info->rx_attr->msg_order);
    fprintf(stdout, "\t\t rx_attr->op_flags %llu\n", info->rx_attr->op_flags);
    fprintf(stdout, "\t\t rx_attr->size %zd\n", info->rx_attr->size);
    fprintf(stdout, "\t\t rx_attr->total_buffered_recv %zd\n",
            info->rx_attr->total_buffered_recv);
    /*/////////////// END rx_attr /////////////////////////////// */

    fprintf(stdout, "\t addr_format %d\n", info->addr_format);

    /*/////////////// ep_attr ////////////////////////////// */
    fprintf(stdout, "\t\t ep_attr->auth_key %u\n", *info->ep_attr->auth_key);
    fprintf(stdout, "\t\t ep_attr->auth_key_size %zd\n",
            info->ep_attr->auth_key_size);
    fprintf(stdout, "\t\t ep_attr->max_msg_size %zd\n",
            info->ep_attr->max_msg_size);
    fprintf(stdout, "\t\t ep_attr->max_order_raw_size %zd\n",
            info->ep_attr->max_order_raw_size);
    fprintf(stdout, "\t\t ep_attr->max_order_war_size %zd\n",
            info->ep_attr->max_order_war_size);
    fprintf(stdout, "\t\t ep_attr->max_order_waw_size %zd\n",
            info->ep_attr->max_order_waw_size);
    fprintf(stdout, "\t\t ep_attr->mem_tag_format %llu\n",
            info->ep_attr->mem_tag_format);
    fprintf(stdout, "\t\t ep_attr->msg_prefix_size %zd\n",
            info->ep_attr->msg_prefix_size);
    fprintf(stdout, "\t\t ep_attr->protocol %d\n", info->ep_attr->protocol);
    fprintf(stdout, "\t\t ep_attr->protocol_version %d\n",
            info->ep_attr->protocol_version);
    fprintf(stdout, "\t\t ep_attr->rx_ctx_cnt %zd\n",
            info->ep_attr->rx_ctx_cnt);
    fprintf(stdout, "\t\t ep_attr->tx_ctx_cnt %zd\n",
            info->ep_attr->tx_ctx_cnt);
    fprintf(stdout, "\t\t ep_attr->type %d\n", info->ep_attr->type);
    /*/////////////// END ep_attr /////////////////////////////// */

    fprintf(stdout, "\t src_addrlen %zd\n", info->src_addrlen);

    /*/////////////// domain_attr ////////////////////////////// */
    fprintf(stdout, "\t\t domain_attr->auth_key %u\n",
            *info->domain_attr->auth_key);
    fprintf(stdout, "\t\t domain_attr->auth_key_size %zd\n",
            info->domain_attr->auth_key_size);
    fprintf(stdout, "\t\t domain_attr->av_type %d\n",
            info->domain_attr->av_type);
    fprintf(stdout, "\t\t domain_attr->caps %llu\n", info->domain_attr->caps);
    fprintf(stdout, "\t\t domain_attr->cntr_cnt %zd\n",
            info->domain_attr->cntr_cnt);
    fprintf(stdout, "\t\t domain_attr->control_progress %d\n",
            info->domain_attr->control_progress);
    fprintf(stdout, "\t\t domain_attr->cq_cnt %zd\n",
            info->domain_attr->cq_cnt);
    fprintf(stdout, "\t\t domain_attr->cq_data_size %zd\n",
            info->domain_attr->cq_data_size);
    fprintf(stdout, "\t\t domain_attr->data_progress %d\n",
            info->domain_attr->data_progress);
    fprintf(stdout, "\t\t domain_attr->ep_cnt %zd\n",
            info->domain_attr->ep_cnt);
    fprintf(stdout, "\t\t domain_attr->max_ep_rx_ctx %zd\n",
            info->domain_attr->max_ep_rx_ctx);
    fprintf(stdout, "\t\t domain_attr->max_ep_srx_ctx %zd\n",
            info->domain_attr->max_ep_srx_ctx);
    fprintf(stdout, "\t\t domain_attr->max_ep_stx_ctx %zd\n",
            info->domain_attr->max_ep_stx_ctx);
    fprintf(stdout, "\t\t domain_attr->max_ep_tx_ctx %zd\n",
            info->domain_attr->max_ep_tx_ctx);
    fprintf(stdout, "\t\t domain_attr->max_err_data %zd\n",
            info->domain_attr->max_err_data);
    fprintf(stdout, "\t\t domain_attr->mode %llu\n", info->domain_attr->mode);
    fprintf(stdout, "\t\t domain_attr->mr_cnt %zd\n",
            info->domain_attr->mr_cnt);
    fprintf(stdout, "\t\t domain_attr->mr_iov_limit %zd\n",
            info->domain_attr->mr_iov_limit);
    fprintf(stdout, "\t\t domain_attr->mr_key_size %zd\n",
            info->domain_attr->mr_key_size);
    fprintf(stdout, "\t\t domain_attr->mr_mode %d\n",
            info->domain_attr->mr_mode);
    fprintf(stdout, "\t\t domain_attr->name %s\n", info->domain_attr->name);
    fprintf(stdout, "\t\t domain_attr->resource_mgmt %d\n",
            info->domain_attr->resource_mgmt);
    fprintf(stdout, "\t\t domain_attr->rx_ctx_cnt %zd\n",
            info->domain_attr->rx_ctx_cnt);
    fprintf(stdout, "\t\t domain_attr->threading %d\n",
            info->domain_attr->threading);
    fprintf(stdout, "\t\t domain_attr->tx_ctx_cnt %zd\n",
            info->domain_attr->tx_ctx_cnt);
    /*/////////////// END domain_attr /////////////////////////////// */

    fprintf(stdout, "\t dest_addrlen %zd\n", info->dest_addrlen);

    /*/////////////// fabric_attr ////////////////////////////// */
    fprintf(stdout, "\t\t fabric_attr->api_version %d\n",
            info->fabric_attr->api_version);
    fprintf(stdout, "\t\t fabric_attr->name %s\n", info->fabric_attr->name);
    fprintf(stdout, "\t\t fabric_attr->prov_name %s\n",
            info->fabric_attr->prov_name);
    fprintf(stdout, "\t\t fabric_attr->prov_version %d\n",
            info->fabric_attr->prov_version);
    /*/////////////// END fabric_attr /////////////////////////////// */

    info = info->next;
    ++count;
  }
  fprintf(stdout, "\nEND of dumping a fi_info object ...\n");
}

uint64_t Provider::requested_key = 0;

std::unique_ptr<Provider> Provider::prov;

int Provider::vector = 0;
} // namespace tl_libfabric
