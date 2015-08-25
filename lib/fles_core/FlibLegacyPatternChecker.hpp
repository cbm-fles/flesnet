// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "PatternChecker.hpp"

class FlibLegacyPatternChecker : public PatternChecker
{
public:
    virtual bool check(const fles::Microslice& m) override;
    virtual void reset() override
    {
        frame_number_ = 0;
        pgen_sequence_number_ = 0;
    };

private:
    bool check_cbmnet_frames(const uint16_t* content, size_t size,
                             uint8_t sys_id, uint8_t sys_ver);
    bool check_content_pgen(const uint16_t* content, size_t size);

    uint8_t frame_number_ = 0;
    uint16_t pgen_sequence_number_ = 0;
};
