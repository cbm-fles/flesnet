// Copyright 2020 Farouk Salem <salem@zib.de>

/**
 * An implementation of generic services of collective operations
 */
#pragma once

#include "ConstVariables.hpp"
#include "LibfabricCollectiveEPInfo.hpp"
#include "LibfabricContextPool.hpp"
#include "LibfabricException.hpp"
#include "Provider.hpp"
#include "SizedMap.hpp"
#include "log.hpp"

#include <rdma/fabric.h>
//#include <rdma/fi_collective.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>
#include <vector>

namespace tl_libfabric {

class LibfabricCollective {
public:
  bool add_endpoint(uint32_t index,
                    struct fi_info* info,
                    const std::string hostname,
                    bool root_ep);

  bool deactive_endpoint(uint32_t index);

protected:
  ~LibfabricCollective();
  LibfabricCollective(uint32_t remote_index, struct fid_domain* pd);

  LibfabricCollective& operator=(const LibfabricCollective&) = delete;
  LibfabricCollective(const LibfabricCollective&) = delete;

  void wait_for_recv_cq(uint32_t events);

  void wait_for_send_cq(uint32_t events);

  std::vector<struct LibfabricCollectiveEPInfo*> retrieve_endpoint_list();

private:
  void initialize_cq(struct fid_cq** cq);

  void initialize_av();

  struct fi_info* get_info(uint32_t index,
                           enum fi_ep_type ep_type,
                           const std::string prov_name,
                           const std::string hostname);

  void create_endpoint(struct LibfabricCollectiveEPInfo* ep_info,
                       enum fi_ep_type ep_type,
                       const std::string prov_name,
                       const std::string hostname);

  fi_addr_t add_av(struct fi_info* info);

  void create_recv_mr(LibfabricCollectiveEPInfo* ep_info);

  void create_send_mr(LibfabricCollectiveEPInfo* ep_info);

  void wait_for_cq(struct fid_cq* cq, const int32_t events);

  std::vector<LibfabricCollectiveEPInfo*> endpoint_list_;

  uint32_t remote_index_;
  // Libfabric
  struct fid_domain* pd_ = nullptr;
  struct fid_cq* recv_cq_ = nullptr;
  struct fid_cq* send_cq_ = nullptr;
  struct fid_av* av_ = nullptr;
  const uint32_t num_cqe_ = 1000000;
};
} // namespace tl_libfabric
