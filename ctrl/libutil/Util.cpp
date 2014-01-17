// $Id: Util.cpp 15 2013-08-06 16:20:18Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include <iomanip>

#include "control/ControlProtocol.hpp"
#include "Util.hpp"

using namespace std;

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
//! FIXME_docs

std::string Status2String(uint32_t status)
{
  const char* str = "";
  switch(status) {
  case kStatusOK:       str = "OK"; break;
  case kStatusNotYet:   str = "Not yet done"; break;
  case kStatusFail:     str = "FAILed"; break;
  case kStatusFailRMC:  str = "FAILed remote message call"; break;
  default:              str = "?status?"; break;
  }  
  return string(str);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

std::string Type2String(uint32_t type)
{
  const char* str = "";
  switch(type) {
  case kOperRead:       str = "r"; break;
  case kOperWrite:      str = "w"; break;
  default:              str = "?"; break;
  }
  return string(str);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

std::string Type2Marker(uint32_t type)
{
  const char* str = "";
  switch(type) {
  case kOperRead:       str = "->"; break;
  case kOperWrite:      str = "<-"; break;
  default:              str = "??"; break;
  }
  return string(str);
}

} // end namespace CbmNet
