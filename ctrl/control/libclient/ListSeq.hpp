// $Id: ListSeq.hpp 18 2013-08-10 14:22:00Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#ifndef included_CbmNet_ListSeq
#define included_CbmNet_ListSeq 1

#include <cstddef>
//#include <cstdint>  --> avoid, needs -std=c++0x -> rootcint problems
#include <stdint.h>
#include <vector>
#include <iostream>

#include "control/ControlProtocol.hpp"
#include "ListSeqItem.hpp"

namespace CbmNet {

  class ListSeq {
    public:
      typedef std::vector<ListSeqItem>::iterator iterator;
      typedef std::vector<ListSeqItem>::const_iterator const_iterator;

                    ListSeq();
                    ListSeq(size_t noper, uint32_t* ptype, 
                            uint32_t* paddr, uint32_t* pvalue);
      virtual      ~ListSeq();

      void          AddOper(const ListSeqItem& oper)
                     { fList.push_back(oper); }
      void          AddOper(uint32_t type, uint32_t addr, uint32_t value=0)
                     { fList.push_back(ListSeqItem(type,addr,value)); }

      void          AddRead(uint32_t addr)
                     { fList.push_back(ListSeqItem(kOperRead,addr)); }
      void          AddWrite(uint32_t addr, uint32_t value)
                     { fList.push_back(ListSeqItem(kOperWrite,addr,value)); }
      
      void          AddReadBlock(uint32_t addr, size_t nval, int32_t addrinc=0);
      void          AddWriteBlock(uint32_t addr, 
                                  const std::vector<uint32_t>& values,
                                  int32_t addrinc=0);
      void          AddWriteBlock(uint32_t addr, const uint32_t* pvalues, 
                                  size_t nval, int32_t addrinc=0);

      void          GetReadBlock(std::vector<uint32_t>& values);
      void          GetReadBlock(uint32_t* pvalues);

      void          Clear();    
      size_t        Size() const 
                      { return fList.size(); }

      size_t        NDone() const 
                      { return fNDone; }
      uint32_t      Status() const
                      { return fStatus; }
      uint32_t      Nodeid() const
                      { return fNodeid; }

      void          SetNDone(size_t ndone)
                      { fNDone = ndone; }
      void          SetStatus(uint32_t status)
                      { fStatus = status; }
      void          SetNodeid(uint32_t nodeid)
                      { fNodeid = nodeid; }

      iterator      Begin()
                      { return fList.begin(); }
      const_iterator Begin() const
                      { return fList.begin(); }
      iterator      End()
                      { return fList.end(); }
      const_iterator End() const
                      { return fList.end(); }

      ListSeqItem&  operator[](size_t n) 
                      { return fList[n]; }
      const ListSeqItem&  operator[](size_t n) const 
                      { return fList[n]; }

      void Dump(std::ostream& os) const;
    
   protected:
     uint32_t       fNodeid;
     uint32_t       fStatus;
     size_t         fNDone;
     std::vector<ListSeqItem>  fList;
  };
  
} // end namespace CbmNet

#endif
