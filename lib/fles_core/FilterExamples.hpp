// Copyright 2016 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Example microslice stream filters based on fles::Filter.
#pragma once

#include "Filter.hpp"
#include "Microslice.hpp"
#include "StorableMicroslice.hpp"

namespace fles
{

// Example filter 1: Override system ID in descriptor
class DescriptorOverrideFilter : public Filter<Microslice, StorableMicroslice>
{
public:
    uint8_t sys_id_;
    uint8_t sys_ver_;

    DescriptorOverrideFilter(uint8_t sys_id, uint8_t sys_ver)
        : sys_id_(sys_id), sys_ver_(sys_ver)
    {
    }

    virtual std::pair<std::unique_ptr<StorableMicroslice>, bool>
    exchange_item(std::shared_ptr<const Microslice> item) override
    {
        if (!item) {
            return std::make_pair(std::unique_ptr<StorableMicroslice>(nullptr),
                                  false);
        }
        auto m = new StorableMicroslice(*item);

        // Modify microslice descriptor
        m->desc().sys_id = sys_id_;
        m->desc().sys_ver = sys_ver_;

        return std::make_pair(std::unique_ptr<StorableMicroslice>(m), false);
    }
};

// Example filter 2: Combine microslices
class CombineContentsFilter
    : public BufferingFilter<Microslice, StorableMicroslice>
{
private:
    void process() override
    {
        // combine the contents of two consecutive microslices
        while (this->input.size() >= 2) {
            auto item1 = input.front();
            this->input.pop_front();
            auto item2 = input.front();
            this->input.pop_front();

            MicrosliceDescriptor desc = item1->desc();
            desc.size += item2->desc().size;
            std::vector<uint8_t> content;
            content.assign(item1->content(),
                           item1->content() + item1->desc().size);
            content.insert(content.end(), item2->content(),
                           item2->content() + item2->desc().size);
            std::unique_ptr<StorableMicroslice> combined(
                new StorableMicroslice(desc, content));
            output.push(std::move(combined));
        }
    }
};
} // namespace fles
