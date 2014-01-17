// $Id: ConvertInt.hpp 24 2013-08-13 18:12:56Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include <string>
#include <sstream>

#include "RerrMsg.hpp"

#ifndef included_CbmNet_ConvertInt
#define included_CbmNet_ConvertInt 1

namespace CbmNet {

template <class T>
  inline bool ConvertInt(const std::string& str, T& val, RerrMsg emsg, 
                         int base=0)
{
  std::istringstream iss(str);
  iss >> std::setbase(base) >> val;
  bool error = !iss;
  std::string rest;
  iss >> rest;
  if (error || rest.size()) {
    emsg.Init("ConvertInt()", 
              std::string("conversion error for '") + str + 
              "' near '" + rest + "'");
    return false;
  }
  return true;
}

} // end namespace CbmNet

#endif
