// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>

#include "PatternChecker.hpp"
#include "FlesnetPatternChecker.hpp"
#include "FlibLegacyPatternChecker.hpp"
#include "FlibPatternChecker.hpp"
#include "FlimPatternChecker.hpp"

std::unique_ptr<PatternChecker> PatternChecker::create(uint8_t arg_sys_id,
                                                       uint8_t arg_sys_ver,
                                                       size_t component) {
  auto sys_id = static_cast<fles::Subsystem>(arg_sys_id);
  auto sys_ver = static_cast<fles::SubsystemFormatFLES>(arg_sys_ver);

  using sfmtfles = fles::SubsystemFormatFLES;

  PatternChecker* pc = nullptr;

  if (sys_id != fles::Subsystem::FLES) {
    pc = new GenericPatternChecker();
  } else {
    switch (sys_ver) {
    case sfmtfles::BasicRampPattern:
      pc = new FlesnetPatternChecker(component);
      break;
    case sfmtfles::CbmNetPattern:
    case sfmtfles::CbmNetFrontendEmulation:
      pc = new FlibLegacyPatternChecker();
      break;
    case sfmtfles::FlibPattern:
      pc = new FlibPatternChecker();
      break;
    case sfmtfles::FlimPattern:
      pc = new FlimPatternChecker();
      break;
    default:
      pc = new GenericPatternChecker();
    }
  }

  return std::unique_ptr<PatternChecker>(pc);
}
