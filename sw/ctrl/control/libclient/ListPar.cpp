// $Id: ListPar.cpp 21 2013-08-11 10:38:57Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//

#include <iomanip>

#include "libutil/Util.hpp"
#include "libutil/RosPrintf.hpp"
#include "control/ControlProtocol.hpp"
#include "ListPar.hpp"

using namespace std;

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
//! Default constructor

ListPar::ListPar()
  : fList()
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

ListPar::ListPar(size_t noper, uint32_t* pnodeid, uint32_t* ptype, 
                 uint32_t* paddr, uint32_t* pvalue)
  : fList()
{
  for (size_t i=0; i<noper; i++) {
    fList.push_back(ListParItem(*pnodeid++, *ptype++, *paddr++, *pvalue++));
  }
}

//------------------------------------------+-----------------------------------
//! Destructor

ListPar::~ListPar()
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListPar::GetValues(uint32_t* pvalues)
{
  ListParItem* pitem = fList.data();
  for (size_t i=0; i<fList.size(); i++) {
    *pvalues++ = (*pitem++).value;
  }  
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListPar::GetStatus(uint32_t* pstatus)
{
  ListParItem* pitem = fList.data();
  for (size_t i=0; i<fList.size(); i++) {
    *pstatus++ = (*pitem++).status;
  }  
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListPar::Clear()
{
  fList.clear();
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void ListPar::Dump(std::ostream& os) const
{
  os << "     ind :   nodeid typ      addr     val-hex     val-dec  : status" 
     << endl;
  for (size_t i=0; i<Size(); i++) {
    const ListParItem& item(fList[i]);
    os << "    " << RosPrintf(i,"d",4)
       << " : " << RosPrintf(item.nodeid,"x0",8)
       << " " << setw(3) << Type2String(item.type) << setw(0)
       << "  " << RosPrintf(item.addr,"x0",8)
       << " " <<  Type2Marker(item.type)
       << " " << RosPrintf(item.value,"x0",8) 
       << " (" << RosPrintf(item.value,"d",10) << ")"
       << " : " << RosPrintf(item.status,"d",2)
       << " '" << Status2String(item.status) << "'"
       << endl;
  }
  return;
}



} // end namespace CbmNet
