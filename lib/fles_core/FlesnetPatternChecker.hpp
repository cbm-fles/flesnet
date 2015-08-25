// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "PatternChecker.hpp"

class FlesnetPatternChecker : public PatternChecker
{
public:
    FlesnetPatternChecker(std::size_t arg_component)
        : component(arg_component){};

    virtual bool check(const fles::MicrosliceView m) override;

private:
    std::size_t component = 0;
};
