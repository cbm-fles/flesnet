// Copyright 2016 Thorsten Schuett <schuett@zib.de>, Farouk Salem <salem@zib.de>

#pragma once

#include <log.hpp>
#include <memory>
#include <rdma/fabric.h>
#include <utility>

#include <vector>

namespace tl_libfabric {
class Provider {
public:
  Provider() = default;

  virtual ~Provider() = default;

  Provider(const Provider&) = delete;
  Provider& operator=(const Provider&) = delete;

  virtual bool has_av() const { return false; };
  virtual bool has_eq_at_eps() const { return true; };
  virtual bool is_connection_oriented() const { return true; };

  virtual struct fi_info* get_info() = 0;
  virtual struct fid_fabric* get_fabric() = 0;
  virtual void accept(struct fid_pep* pep,
                      const std::string& hostname,
                      unsigned short port,
                      unsigned int count,
                      fid_eq* eq) = 0;

  virtual void connect(::fid_ep* ep,
                       uint32_t max_send_wr,
                       uint32_t max_send_sge,
                       uint32_t max_recv_wr,
                       uint32_t max_recv_sge,
                       uint32_t max_inline_data,
                       const void* param,
                       size_t paramlen,
                       void* addr) = 0;

  static void init(const std::string& local_host_name) {
    prov = get_provider(local_host_name);
  }

  virtual void set_hostnames_and_services(
      struct fid_av* /*av*/,
      const std::vector<std::string>& /*compute_hostnames*/,
      const std::vector<std::string>& /*compute_services*/,
      std::vector<::fi_addr_t>& /*fi_addrs*/) = 0;

  static std::unique_ptr<Provider>& getInst() { return prov; }

  static uint64_t requested_key;

  static int vector;

private:
  static std::unique_ptr<Provider>
  get_provider(const std::string& local_host_name);
  static std::unique_ptr<Provider> prov;
};
} // namespace tl_libfabric
