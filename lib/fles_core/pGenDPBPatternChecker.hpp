// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "PatternChecker.hpp"
#define MAX_FIFO_NUM_PER_LINK		48    //for each flim link, there may be max. 48 fifo to inject data
#define MAX_FLIM_ID            16    // there are max. 16 flim links for one AFCK
#define DEFAULT_DATA_HEAD			0x7F  // default data header

class PgenDpbPatternChecker : public PatternChecker
{
public:
    bool check(const fles::Microslice& m) override;
    void reset() override;

private:
    uint64_t pgen_dpb_flim_id_;
};
