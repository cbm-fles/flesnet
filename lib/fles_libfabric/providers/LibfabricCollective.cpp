// Copyright 2020 Farouk Salem <salem@zib.de>

#include "LibfabricCollective.hpp"

namespace tl_libfabric {

LibfabricCollective::LibfabricCollective(uint32_t remote_index,
                                         struct fid_domain* pd)
    : remote_index_(remote_index), pd_(pd) {

  initialize_cq(&recv_cq_);
  assert(recv_cq_ != nullptr);
  initialize_cq(&send_cq_);
  assert(send_cq_ != nullptr);
  // av
  if (Provider::getInst()->has_av()) {
    initialize_av();
  }
}

void LibfabricCollective::initialize_cq(struct fid_cq** cq) {
  L_(debug) << "initialize_cq ...";
  struct fi_cq_attr cq_attr;
  memset(&cq_attr, 0, sizeof(cq_attr));
  cq_attr.size = num_cqe_;
  cq_attr.flags = 0;
  cq_attr.format = FI_CQ_FORMAT_CONTEXT;
  cq_attr.wait_obj = FI_WAIT_NONE;
  cq_attr.signaling_vector = Provider::vector++; // ??
  cq_attr.wait_cond = FI_CQ_COND_NONE;
  cq_attr.wait_set = nullptr;
  int res = fi_cq_open(pd_, &cq_attr, cq, nullptr);
  if (*cq == nullptr) {
    L_(fatal) << "fi_cq_open failed: " << res << "=" << fi_strerror(-res);
    throw LibfabricException("fi_cq_open failed");
  }
}

void LibfabricCollective::initialize_av() {
  struct fi_av_attr av_attr;

  memset(&av_attr, 0, sizeof(av_attr));
  av_attr.type = FI_AV_TABLE;
  av_attr.count = 1000;
  assert(av_ == nullptr);
  int res = fi_av_open(pd_, &av_attr, &av_, NULL);
  assert(res == 0);
  if (av_ == nullptr) {
    L_(fatal) << "fi_av_open failed: " << res << "=" << fi_strerror(-res);
    throw LibfabricException("fi_av_open failed");
  }
}

LibfabricCollective::~LibfabricCollective() {}

bool LibfabricCollective::add_endpoint(uint32_t index,
                                       struct fi_info* fiinfo,
                                       const std::string hostname,
                                       bool root_ep) {

  L_(debug) << "add_endpoint " << index << ":" << hostname << " root "
            << root_ep;
  assert(index == endpoint_list_.size());

  struct LibfabricCollectiveEPInfo* ep_info = new LibfabricCollectiveEPInfo();
  ep_info->index = index;
  ep_info->root_ep = root_ep;
  create_endpoint(ep_info, fiinfo->ep_attr->type,
                  fiinfo->fabric_attr->prov_name, hostname);
  create_recv_mr(ep_info);
  create_send_mr(ep_info);
  endpoint_list_.push_back(ep_info);
  return true;
}

fi_addr_t LibfabricCollective::add_av(struct fi_info* fiinfo) {
  fi_addr_t fi_addr;
  assert(fiinfo->dest_addr != nullptr);
  int res = fi_av_insert(av_, fiinfo->dest_addr, 1, &fi_addr, 0, NULL);
  assert(res == 1);
  return fi_addr;
}

void LibfabricCollective::create_endpoint(
    struct LibfabricCollectiveEPInfo* ep_info,
    enum fi_ep_type ep_type,
    const std::string prov_name,
    const std::string hostname) {
  struct fi_info* fiinfo =
      get_info(ep_info->index, ep_type, prov_name, hostname);
  assert(fiinfo != nullptr);
  if (hostname != "")
    ep_info->fi_addr = add_av(fiinfo);
  int err = fi_endpoint(pd_, fiinfo, &ep_info->ep, this);
  if (err != 0) {
    L_(fatal) << "fi_endpoint failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_endpoint failed");
  }
  // bindings
  assert(ep_info->ep != nullptr);
  assert(recv_cq_ != nullptr);
  err = fi_ep_bind(ep_info->ep, &recv_cq_->fid, FI_RECV);
  if (err != 0) {
    L_(fatal) << "fi_ep_bind failed (recv_cq_): " << err << "="
              << fi_strerror(-err);
    throw LibfabricException("fi_ep_bind failed (recv_cq_)");
  }
  err = fi_ep_bind(ep_info->ep, &send_cq_->fid, FI_SEND);
  if (err != 0) {
    L_(fatal) << "fi_ep_bind failed (send_cq_): " << err << "="
              << fi_strerror(-err);
    throw LibfabricException("fi_ep_bind failed (send_cq_)");
  }
  if (Provider::getInst()->has_av()) {
    err = fi_ep_bind(ep_info->ep, &av_->fid, 0);
    if (err != 0) {
      L_(fatal) << "fi_ep_bind failed (av): " << err << "="
                << fi_strerror(-err);
      throw LibfabricException("fi_ep_bind failed (av)");
    }
  }
  err = fi_enable(ep_info->ep);
  if (err != 0) {
    L_(fatal) << "fi_enable failed: " << err << "=" << fi_strerror(-err);
    throw LibfabricException("fi_enable failed");
  }
  size_t addr_len = sizeof(ep_info->send_buffer);
  err = fi_getname(&ep_info->ep->fid, &ep_info->send_buffer, &addr_len);
  assert(err == 0);
}

struct fi_info* LibfabricCollective::get_info(uint32_t index,
                                              enum fi_ep_type ep_type,
                                              const std::string prov_name,
                                              const std::string hostname) {
  // TODO update port
  uint32_t port = (hostname == "" ? 14195 + remote_index_ * 2 + index
                                  : 14195 + remote_index_ + index * 2);

  struct fi_info* info = nullptr;
  // TODO hard-coded ep_type
  assert(ep_type == FI_EP_RDM);
  struct fi_info* hints = Provider::get_hints(ep_type, prov_name);
  int err;
  // TODO define the port in config file
  if (hostname == "") {
    err = fi_getinfo(FIVERSION, nullptr, std::to_string(port).c_str(),
                     FI_SOURCE, hints, &info);
  } else {
    err = fi_getinfo(FIVERSION, hostname.c_str(), std::to_string(port).c_str(),
                     FI_NUMERICHOST, hints, &info);
  }
  if (err != 0) {
    L_(fatal) << "fi_getinfo failed in LibfabricCollective::get_info: " << err
              << "=" << fi_strerror(-err);
    throw LibfabricException(
        "fi_getinfo failed in LibfabricCollective::get_info");
  }

  fi_freeinfo(hints);
  return info;
}

void LibfabricCollective::create_recv_mr(LibfabricCollectiveEPInfo* ep_info) {

  // register memory regions
  int err = fi_mr_reg(pd_, &ep_info->recv_buffer, sizeof(ep_info->recv_buffer),
                      FI_RECV | FI_TAGGED, 0, Provider::requested_key++, 0,
                      &ep_info->mr_recv, nullptr);
  if (err != 0) {
    L_(fatal)
        << "fi_mr_reg failed for create_recv_mr msg in LibfabricCollective: "
        << err << "=" << fi_strerror(-err);
    throw LibfabricException(
        "fi_mr_reg failed for create_recv_mr msg in LibfabricCollective");
  }

  // prepare recv message
  ep_info->recv_sge.iov_base = &ep_info->recv_buffer;
  ep_info->recv_sge.iov_len = sizeof(ep_info->recv_buffer);

  void* recv_wr_descs[1] = {nullptr};
  recv_wr_descs[0] = fi_mr_desc(ep_info->mr_recv);

  struct fi_custom_context* context =
      LibfabricContextPool::getInst()->getContext();
  context->op_context = ep_info->index;

  ep_info->recv_msg_wr.msg_iov = &ep_info->recv_sge;
  ep_info->recv_msg_wr.desc = recv_wr_descs;
  ep_info->recv_msg_wr.iov_count = 1;
  ep_info->recv_msg_wr.addr = ep_info->fi_addr;
  ep_info->recv_msg_wr.context = context;
  ep_info->recv_msg_wr.data = 0;
}

void LibfabricCollective::create_send_mr(LibfabricCollectiveEPInfo* ep_info) {

  // register memory regions
  int err = fi_mr_reg(pd_, &ep_info->send_buffer, sizeof(ep_info->send_buffer),
                      FI_SEND | FI_TAGGED, 0, Provider::requested_key++, 0,
                      &ep_info->mr_send, nullptr);
  if (err != 0) {
    L_(fatal)
        << "fi_mr_reg failed for create_send_mr msg in LibfabricCollective: "
        << err << "=" << fi_strerror(-err);
    throw LibfabricException(
        "fi_mr_reg failed for create_send_mr msg in LibfabricCollective");
  }

  // prepare recv message
  ep_info->send_sge.iov_base = &ep_info->send_buffer;
  ep_info->send_sge.iov_len = sizeof(ep_info->send_buffer);

  void* send_wr_descs[1] = {nullptr};
  send_wr_descs[0] = fi_mr_desc(ep_info->mr_send);

  struct fi_custom_context* context =
      LibfabricContextPool::getInst()->getContext();
  context->op_context = ep_info->index;

  ep_info->send_msg_wr.msg_iov = &ep_info->send_sge;
  ep_info->send_msg_wr.desc = send_wr_descs;
  ep_info->send_msg_wr.iov_count = 1;
  ep_info->send_msg_wr.addr = ep_info->fi_addr;
  ep_info->send_msg_wr.context = context;
  ep_info->send_msg_wr.data = 0;
}

bool LibfabricCollective::deactive_endpoint(uint32_t index) {
  assert(index < endpoint_list_.size());
  struct LibfabricCollectiveEPInfo* ep_info = endpoint_list_[index];
  assert(ep_info->index == index);
  ep_info->active = false;
  fi_close(&ep_info->ep->fid);
  return true;
}
void LibfabricCollective::wait_for_cq(struct fid_cq* cq, const int32_t events) {

  struct fi_cq_entry wc[events];
  int ne, received = 0;

  while (received < events) {
    ne = fi_cq_read(cq, &wc, (events - received));
    if ((ne < 0) && (ne != -FI_EAGAIN)) {
      L_(fatal) << "wait_for_cq failed: " << ne << "=" << fi_strerror(-ne);
      throw LibfabricException("wait_for_cq failed");
    }

    if (ne == -FI_EAGAIN || ne == 0) {
      continue;
    }

    L_(debug) << "got " << ne << " events";
    received += ne;
    for (int i = 0; i < ne; i++) {
      struct fi_custom_context* context =
          static_cast<struct fi_custom_context*>(wc[i].op_context);
      assert(context != nullptr);
      if (endpoint_list_[context->op_context]->fi_addr == FI_ADDR_UNSPEC) {
        int res = fi_av_insert(
            av_, &endpoint_list_[context->op_context]->recv_buffer, 1,
            &endpoint_list_[context->op_context]->fi_addr, 0, NULL);
        assert(res == 1);
        endpoint_list_[context->op_context]->recv_msg_wr.addr =
            endpoint_list_[context->op_context]->send_msg_wr.addr =
                endpoint_list_[context->op_context]->fi_addr;
      }
    }
  }
}

void LibfabricCollective::wait_for_recv_cq(uint32_t events) {
  wait_for_cq(recv_cq_, events);
}

void LibfabricCollective::wait_for_send_cq(uint32_t events) {
  wait_for_cq(send_cq_, events);
}

std::vector<struct LibfabricCollectiveEPInfo*>
LibfabricCollective::retrieve_endpoint_list() {
  return endpoint_list_;
}

} // namespace tl_libfabric
