// Copyright 2016 Thorsten Schuett <schuett@zib.de>
#pragma once

#include <memory>
#include <unordered_map>
#include <rdma/fabric.h>

class Provider
{
public:
    Provider()
    {
    }

    virtual ~Provider() {};

    Provider(const Provider &) = delete;
    Provider &operator=(const Provider &) = delete;

    virtual struct fi_info *get_info() = 0;
    virtual struct fid_fabric *get_fabric() = 0;
    virtual void accept(struct fid_pep *pep, const std::string &hostname,unsigned short port,
                        unsigned int count, fid_eq *eq) = 0;

    virtual void connect(fid_ep* ep, uint32_t max_send_wr,
                         uint32_t max_send_sge, uint32_t max_recv_wr,
                         uint32_t max_recv_sge, uint32_t max_inline_data,
                         const void* param, size_t paramlen, void* addr) = 0;

    static void init(std::string local_host_name)
    {
      prov = get_provider(local_host_name);
    }

    static std::unique_ptr<Provider> &getInst()
    {
        return prov;
    }

    static uint64_t requested_key;

private:
    static std::unique_ptr<Provider> get_provider(std::string local_host_name);
    static std::unique_ptr<Provider> prov;
};
