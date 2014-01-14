// $Id: RosPrintBvi.ipp 20 2013-08-10 16:42:54Z mueller $
//
// Copyright 2011- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
//
// This program is free software; you may redistribute and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 2, or at your option any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for complete details.
// 
// Imported Rev 488/366 1.0.2 from Retro -> CbmNet
// ---------------------------------------------------------------------------

/*!
  \file
  \version $Id: RosPrintBvi.ipp 20 2013-08-10 16:42:54Z mueller $
  \brief   Implemenation (inline) of RosPrintBvi.
*/

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
/*! 
  \relates RosPrintBvi
  \brief ostream insertion operator.
*/

inline std::ostream& operator<<(std::ostream& os, const RosPrintBvi& obj)
{
  obj.Print(os);
  return os;
}

//------------------------------------------+-----------------------------------
/*! 
  \relates RosPrintBvi
  \brief string insertion operator.
*/

inline std::string& operator<<(std::string& os, const RosPrintBvi& obj)
{
  obj.Print(os);
  return os;
}


} // end namespace CbmNet
