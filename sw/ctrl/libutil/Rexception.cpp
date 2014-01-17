// $Id: Rexception.cpp 20 2013-08-10 16:42:54Z mueller $
//
// Copyright 2013- by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
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
// Imported Rev 488/474 1.0 from Retro -> CbmNet
// ---------------------------------------------------------------------------

/*!
  \file
  \version $Id: Rexception.cpp 20 2013-08-10 16:42:54Z mueller $
  \brief   Implemenation of Rexception.
*/

#include "Rexception.hpp"

using namespace std;

/*!
  \class CbmNet::Rexception
  \brief FIXME_docs
*/

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
//! Default constructor

Rexception::Rexception()
  : fErrmsg()
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

Rexception::Rexception(const RerrMsg& errmsg)
  : fErrmsg(errmsg)
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

Rexception::Rexception(const std::string& meth, const std::string& text)
  : fErrmsg(meth,text)
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

Rexception::Rexception(const std::string& meth, const std::string& text, 
                       int errnum)
  : fErrmsg(meth,text,errnum)
{}

//------------------------------------------+-----------------------------------
//! Destructor

Rexception::~Rexception() throw()
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

const char* Rexception::what() const throw()
{
  return fErrmsg.Message().c_str();
}

} // end namespace CbmNet
