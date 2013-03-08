/**
 * \file global.hpp
 *
 * 2012, 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef GLOBAL_HPP
#define GLOBAL_HPP

#include "einhard.hpp"

extern einhard::Logger<(einhard::LogLevel) MINLOGLEVEL, true> out;

class Parameters;
extern Parameters* par;

#endif /* GLOBAL_HPP */
