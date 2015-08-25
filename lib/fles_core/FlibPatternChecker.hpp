// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "PatternChecker.hpp"

class FlibPatternChecker : public PatternChecker
{
public:
    virtual bool check(const fles::Microslice& m) override;
    virtual void reset() override { flib_pgen_packet_number_ = 0; };

private:
    uint32_t flib_pgen_packet_number_ = 0;
};
