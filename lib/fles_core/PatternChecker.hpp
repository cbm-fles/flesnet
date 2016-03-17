// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "Microslice.hpp"
#include <memory>

class PatternChecker
{
public:
    virtual ~PatternChecker() = default;

    virtual bool check(const fles::Microslice& m) = 0;
    virtual void reset(){};

    static std::unique_ptr<PatternChecker>
    create(uint8_t arg_sys_id, uint8_t arg_sys_ver, size_t component);
};

class GenericPatternChecker : public PatternChecker
{
public:
    virtual bool check(const fles::Microslice& /* m */) override
    {
        return true;
    };
};
