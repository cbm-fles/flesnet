// Copyright 2016 Thorsten Schuett <schuett@zib.de>
#pragma once

#include <rdma/fabric.h>

#include "Provider.hpp"

#include <cassert>

class MsgSocketsProvider : public Provider
{
public:
    MsgSocketsProvider(struct fi_info* info);

    MsgSocketsProvider(const MsgSocketsProvider&) = delete;
    MsgSocketsProvider& operator=(const MsgSocketsProvider&) = delete;

    ~MsgSocketsProvider();

    struct fi_info* get_info() override
    {
        assert(info_ != nullptr);
        return info_;
    }

    struct fid_fabric* get_fabric() override { return fabric_; };

    static struct fi_info* exists(std::string local_host_name);

    void accept(struct fid_pep* pep, const std::string& hostname,
                unsigned short port, unsigned int count, fid_eq* eq) override;

    void connect(fid_ep* ep, uint32_t max_send_wr, uint32_t max_send_sge,
                 uint32_t max_recv_wr, uint32_t max_recv_sge,
                 uint32_t max_inline_data, const void* param, size_t paramlen,
                 void* addr) override;

private:
    struct fi_info* info_ = nullptr;
    struct fid_fabric* fabric_ = nullptr;
};
