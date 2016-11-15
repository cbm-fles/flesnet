// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "PatternChecker.hpp"
#define MAX_FIFO_NUM_PER_LINK		48
#define DEFAULT_SYS_ID				0x7F

class pGenDPBPatternChecker : public PatternChecker
{
public:
    bool check(const fles::Microslice& m) override;
    void reset() override;

private:
    uint8_t pgen_dpb_channel_mask_[MAX_FIFO_NUM_PER_LINK];
    uint64_t pgen_dpb_flim_id_;
};
