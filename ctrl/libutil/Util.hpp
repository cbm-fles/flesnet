// $Id: Util.hpp 21 2013-08-11 10:38:57Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include <iostream>
#include <string>

#ifndef included_CbmNet_Util
#define included_CbmNet_Util 1

namespace CbmNet {

  inline uint32_t   MergeTypeAddr(uint32_t type, uint32_t addr)
                      { return (type&0xff)<<24 | (addr&0xffffff); }
  inline uint32_t   GetTypeFromTAddr(uint32_t taddr)
                      { return (taddr>>24)&0xff; }
  inline uint32_t   GetAddrFromTAddr(uint32_t taddr)
                      { return taddr&0xffffff; }
  
  std::string       Status2String(uint32_t status);
  std::string       Type2String(uint32_t type);
  std::string       Type2Marker(uint32_t type);

} // end namespace CbmNet

#endif
