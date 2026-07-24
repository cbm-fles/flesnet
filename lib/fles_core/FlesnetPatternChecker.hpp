// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "PatternChecker.hpp"
#include <optional>

class FlesnetPatternChecker : public PatternChecker {
public:
  bool check(const fles::Microslice& m) override;
  void reset() override { component.reset(); }

private:
  std::optional<uint64_t> component;
};
