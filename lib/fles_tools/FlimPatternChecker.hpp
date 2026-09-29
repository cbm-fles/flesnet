// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
// Copyright 2025 Dirk Hutter <cmail@dirk-hutter.de>
#pragma once

#include "PatternChecker.hpp"

class FlimPatternChecker : public PatternChecker {
public:
  bool check(const fles::Microslice& m) override;
  void reset() override { pgen_packet_number_ = 0; };

private:
  uint32_t pgen_packet_number_ = 0;
};
