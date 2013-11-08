// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "einhard.hpp"
#pragma GCC diagnostic pop

extern einhard::Logger<static_cast<einhard::LogLevel>(MINLOGLEVEL), true> out;
