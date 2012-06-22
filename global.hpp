/**
 * \file global.hpp
 *
 * 2012, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef GLOBAL_HPP
#define GLOBAL_HPP

#include "einhard.hpp"

extern einhard::Logger<(einhard::LogLevel) MINLOGLEVEL, true> Log;

class Parameters;
extern Parameters* Par;

#endif /* GLOBAL_HPP */
