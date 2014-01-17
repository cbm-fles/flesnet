// $Id: RosPrintfBase.ipp 20 2013-08-10 16:42:54Z mueller $
//
// Copyright 2006-2011 by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
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
// Imported Rev 488/357 1.0 from Retro -> CbmNet
// ---------------------------------------------------------------------------

/*!
  \file
  \version $Id: RosPrintfBase.ipp 20 2013-08-10 16:42:54Z mueller $
  \brief   Implemenation (inline) of RosPrintfBase
*/

// all method definitions in namespace CbmNet
namespace CbmNet {

/*!
  \class RosPrintfBase 
  \brief Base class for print objects. **
*/
//------------------------------------------+-----------------------------------
/*!
  \fn CbmNet::RosPrintfBase::ToStream(ostream& os) const
  \brief Concrete implementation of the ostream insertion.
*/

//------------------------------------------+-----------------------------------
/*!
  \brief Constructor.

  \param form   format descriptor string
  \param width  field width
  \param prec   precision
*/

inline RosPrintfBase::RosPrintfBase(const char* form, int width, int prec)
  : fForm(form),
    fWidth(width),
    fPrec(prec)
{}

//------------------------------------------+-----------------------------------
/*!
  \brief Destructor.
*/

inline RosPrintfBase::~RosPrintfBase()
{}

//------------------------------------------+-----------------------------------
/*!
  \relates RosPrintfBase
  \brief ostream insertion
*/

inline std::ostream& operator<<(std::ostream& os, const RosPrintfBase& obj)
{
  obj.ToStream(os);
  return os;
}

} // end namespace CbmNet
