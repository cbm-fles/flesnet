// $Id: ListSeq.cpp 21 2013-08-11 10:38:57Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include <iomanip>

#include "libutil/Util.hpp"
#include "libutil/RosPrintf.hpp"
#include "control/ControlProtocol.hpp"
#include "ListSeq.hpp"

using namespace std;

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
//! Default constructor

ListSeq::ListSeq()
  : fNodeid(0),
    fStatus(0),                             // FIXME_code: proper value
    fNDone(0),
    fList()
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

ListSeq::ListSeq(size_t noper, uint32_t* ptype, 
                 uint32_t* paddr, uint32_t* pvalue)
  : fNodeid(0),
    fStatus(0),                             // FIXME_code: proper value
    fNDone(0),
    fList()
{
  for (size_t i=0; i<noper; i++) {
    fList.push_back(ListSeqItem(*ptype++, *paddr++, *pvalue++));
  }
}

//------------------------------------------+-----------------------------------
//! Destructor

ListSeq::~ListSeq()
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListSeq::AddReadBlock(uint32_t addr, size_t nval, int32_t addrinc)
{
  for (size_t i=0; i<nval; i++) {
    fList.push_back(ListSeqItem(kOperRead, addr));
    addr += addrinc;
  }
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListSeq::AddWriteBlock(uint32_t addr, const std::vector<uint32_t>& values, 
                            int32_t addrinc)
{
  AddWriteBlock(addr, values.data(), values.size(), addrinc);
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListSeq::AddWriteBlock(uint32_t addr, const uint32_t* pvalues, size_t nval,
                            int32_t addrinc)
{
  for (size_t i=0; i<nval; i++) {
    fList.push_back(ListSeqItem(kOperWrite, addr, *pvalues++));
    addr += addrinc;
  }  
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListSeq::GetReadBlock(std::vector<uint32_t>& values)
{
  values.resize(fNDone);
  GetReadBlock(values.data());
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListSeq::GetReadBlock(uint32_t* pvalues)
{
  ListSeqItem* pitem = fList.data();
  for (size_t i=0; i<fNDone; i++) {
    *pvalues++ = (*pitem++).value;
  }  
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListSeq::Clear()
{
  fList.clear();
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListSeq::Dump(std::ostream& os) const
{
  os << "  nodeid=" << RosPrintf(Nodeid(),"x0",8)
     << "  ndone=" << RosPrintf(NDone(),"d",4)
     << "  status=" << RosPrintf(Status(),"d",2)
     << " '" << Status2String(Status()) << "'"
     << endl;

  os << "   ind : typ      addr     val-hex     val-dec  : status" << endl;
  for (size_t i=0; i<Size(); i++) {
    const ListSeqItem& item(fList[i]);
    os << "  " << RosPrintf(i,"d",4)
       << " : " << setw(3) << Type2String(item.type) << setw(0)
       << "  " << RosPrintf(item.addr,"x0",8)
       << " " <<  Type2Marker(item.type)
       << " " << RosPrintf(item.value,"x0",8) 
       << " (" << RosPrintf(item.value,"d",10) << ")" 
       << " : " << (i<NDone() ? "OK" : "FAIL")
       << endl;
  }
  return;
}

} // end namespace CbmNet
