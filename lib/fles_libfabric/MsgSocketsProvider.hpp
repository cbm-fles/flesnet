// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include <rdma/fabric.h>

#include "Provider.hpp"

#include <cassert>

namespace tl_libfabric {

class MsgSocketsProvider : public Provider {
public:
  MsgSocketsProvider(struct fi_info* info);

  MsgSocketsProvider(const MsgSocketsProvider&) = delete;
  MsgSocketsProvider& operator=(const MsgSocketsProvider&) = delete;

  ~MsgSocketsProvider() override;

  struct fi_info* get_info() override {
    assert(info_ != nullptr);
    return info_;
  }

  struct fid_fabric* get_fabric() override {
    return fabric_;
  };

  static struct fi_info* exists(const std::string& local_host_name);

  void accept(struct fid_pep* pep,
              const std::string& hostname,
              unsigned short port,
              unsigned int count,
              fid_eq* eq) override;

  void connect(fid_ep* ep,
               uint32_t max_send_wr,
               uint32_t max_send_sge,
               uint32_t max_recv_wr,
               uint32_t max_recv_sge,
               uint32_t max_inline_data,
               const void* param,
               size_t param_len,
               void* addr) override;

  void set_hostnames_and_services(
      struct fid_av* /*av*/,
      const std::vector<std::string>& /*compute_hostnames*/,
      const std::vector<std::string>& /*compute_services*/,
      std::vector<fi_addr_t>& /*fi_addrs*/) override{};

private:
  struct fi_info* info_ = nullptr;
  struct fid_fabric* fabric_ = nullptr;
};
} // namespace tl_libfabric
