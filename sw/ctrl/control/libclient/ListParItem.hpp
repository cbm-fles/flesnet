// $Id: ListParItem.hpp 18 2013-08-10 14:22:00Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#ifndef included_CbmNet_ListParItem
#define included_CbmNet_ListParItem 1

//#include <cstdint>  --> avoid, needs -std=c++0x -> rootcint problems
#include <stdint.h>

#include "control/ControlProtocol.hpp"

namespace CbmNet {

  struct ListParItem {
    public:
                    ListParItem()
                      : nodeid(0), type(0), addr(0), value(0), 
                        status(kStatusNotYet) 
                      {}
                    ListParItem(uint32_t _nodeid, uint32_t _type, 
                                uint32_t _addr=0, uint32_t _value=0,
                                uint32_t _status=kStatusNotYet)
                      : nodeid(_nodeid), type(_type), addr(_addr), 
                        value(_value), status(_status) 
                      {}

      uint32_t      nodeid;
      uint32_t      type;
      uint32_t      addr;
      uint32_t      value;
      uint32_t      status;
};
  
} // end namespace CbmNet

#endif
