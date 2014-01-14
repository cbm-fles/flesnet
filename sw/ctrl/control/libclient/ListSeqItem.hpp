// $Id: ListSeqItem.hpp 18 2013-08-10 14:22:00Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#ifndef included_CbmNet_ListSeqItem
#define included_CbmNet_ListSeqItem 1

//#include <cstdint>  --> avoid, needs -std=c++0x -> rootcint problems
#include <stdint.h>

namespace CbmNet {

  struct ListSeqItem {
    public:
                    ListSeqItem()
                      : type(0), addr(0), value(0) 
                      {}
                    ListSeqItem(uint32_t _type, uint32_t _addr, 
                                uint32_t _value=0)
                      : type(_type), addr(_addr), value(_value) 
                      {}

      uint32_t      type;
      uint32_t      addr;
      uint32_t      value;
};
  
} // end namespace CbmNet

#endif
