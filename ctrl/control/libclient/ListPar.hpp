// $Id: ListPar.hpp 18 2013-08-10 14:22:00Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#ifndef included_CbmNet_ListPar
#define included_CbmNet_ListPar 1

#include <cstddef>
//#include <cstdint>  --> avoid, needs -std=c++0x -> rootcint problems
#include <stdint.h>
#include <vector>
#include <iostream>

#include "control/ControlProtocol.hpp"
#include "ListParItem.hpp"

namespace CbmNet {

  class ListPar {
    public:

      typedef std::vector<ListParItem>::iterator iterator;
      typedef std::vector<ListParItem>::const_iterator const_iterator;

                    ListPar();
                    ListPar(size_t noper, uint32_t* pnodeid, uint32_t* ptype, 
                            uint32_t* paddr, uint32_t* pvalue);
      virtual      ~ListPar();

      void          AddOper(const ListParItem& oper)
                     { fList.push_back(oper); }
      void          AddOper(uint32_t nodeid, uint32_t type, uint32_t addr, 
                            uint32_t value=0)
                     { fList.push_back(ListParItem(nodeid,type,addr,value)); }

      void          AddRead(uint32_t nodeid, uint32_t addr)
                     { fList.push_back(ListParItem(nodeid,kOperRead,addr)); }
      void          AddWrite(uint32_t nodeid, uint32_t addr, uint32_t value)
                     { fList.push_back(ListParItem(nodeid,kOperWrite,
                                                   addr,value)); }
      
      void          GetValues(uint32_t* pvalues);
      void          GetStatus(uint32_t* pstatus);

      void          Clear();    
      size_t        Size() const 
                      { return fList.size(); }

      iterator      Begin()
                      { return fList.begin(); }
      const_iterator Begin() const
                      { return fList.begin(); }
      iterator      End()
                      { return fList.end(); }
      const_iterator End() const
                      { return fList.end(); }

      ListParItem&  operator[](size_t n) 
                      { return fList[n]; }
      const ListParItem&  operator[](size_t n) const 
                      { return fList[n]; }
 
      void Dump(std::ostream& os) const;

   protected:
     std::vector<ListParItem>  fList;
};
  
} // end namespace CbmNet

#endif
