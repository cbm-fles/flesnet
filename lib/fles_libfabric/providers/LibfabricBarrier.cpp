
#include "LibfabricBarrier.hpp"

namespace tl_libfabric {

void LibfabricBarrier::create_barrier_instance(uint32_t remote_index,
                                               struct fid_domain* pd,
                                               bool is_root) {
  if (barrier_ == nullptr)
    barrier_ = new LibfabricBarrier(remote_index, pd, is_root);
  assert(LibfabricBarrier::barrier_ != nullptr);
}

LibfabricBarrier* LibfabricBarrier::get_instance() { return barrier_; }

LibfabricBarrier::~LibfabricBarrier() {}
LibfabricBarrier::LibfabricBarrier(uint32_t remote_index,
                                   struct fid_domain* pd,
                                   bool is_root)
    : LibfabricCollective(remote_index, pd), root_(is_root) {
  // TODO remote_index needed?
}

size_t LibfabricBarrier::call_barrier() {
  if (root_) {
    receive_all();
    broadcast();
  } else {
    broadcast(true);
    receive_all(true);
  }
  return 0;
}

void LibfabricBarrier::receive_all(bool only_root_eps) {
  std::vector<struct LibfabricCollectiveEPInfo*> endpoints =
      retrieve_endpoint_list();
  uint32_t count = 0;
  assert(endpoints.size() >= 1);
  for (uint32_t i = 0; i < endpoints.size(); i++) {
    if (!endpoints[i]->active || (only_root_eps && !endpoints[i]->root_ep))
      continue;
    recv(endpoints[i]);
    count++;
  }
  wait_for_recv_cq(count);
}

void LibfabricBarrier::broadcast(bool only_root_eps) {
  std::vector<struct LibfabricCollectiveEPInfo*> endpoints =
      retrieve_endpoint_list();
  assert(endpoints.size() > 0);
  uint32_t count = 0;
  for (uint32_t i = 0; i < endpoints.size(); i++) {
    if (!endpoints[i]->active || (only_root_eps && !endpoints[i]->root_ep))
      continue;
    send(endpoints[i]);
    count++;
  }
  wait_for_send_cq(count);
}

void LibfabricBarrier::recv(const struct LibfabricCollectiveEPInfo* ep_info) {
  int err = fi_trecvmsg(ep_info->ep, &ep_info->recv_msg_wr, FI_COMPLETION);
  if (err != 0) {
    L_(fatal) << "fi_recvmsg failed in LibfabricBarrier::recv: "
              << strerror(err);
    throw LibfabricException("fi_recvmsg failed");
  }
}

void LibfabricBarrier::send(const struct LibfabricCollectiveEPInfo* ep_info) {
  int err = fi_tsendmsg(ep_info->ep, &ep_info->send_msg_wr, FI_COMPLETION);
  if (err != 0) {
    L_(fatal) << "fi_tsendmsg failed in LibfabricBarrier::v: " << strerror(err);
    throw LibfabricException("fi_tsendmsg failed");
  }
}

LibfabricBarrier* LibfabricBarrier::barrier_ = nullptr;
} // namespace tl_libfabric
