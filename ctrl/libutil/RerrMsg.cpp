// $Id: RerrMsg.cpp 20 2013-08-10 16:42:54Z mueller $
//
// Copyright 2011-2013 by Walter F.J. Mueller <W.F.J.Mueller@gsi.de>
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
// Imported Rev 493/493 1.2 from Retro -> CbmNet
// ---------------------------------------------------------------------------

/*!
  \file
  \version $Id: RerrMsg.cpp 20 2013-08-10 16:42:54Z mueller $
  \brief   Implemenation of RerrMsg.
*/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "RerrMsg.hpp"

using namespace std;

/*!
  \class CbmNet::RerrMsg
  \brief FIXME_docs
*/

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
//! Default constructor

RerrMsg::RerrMsg()
  : fMeth(),
    fText()
{}

//------------------------------------------+-----------------------------------
//! Copy constructor

RerrMsg::RerrMsg(const RerrMsg& rhs)
  : fMeth(rhs.fMeth),
    fText(rhs.fText)
{}

//------------------------------------------+-----------------------------------
//! Construct from method and message text

RerrMsg::RerrMsg(const std::string& meth, const std::string& text)
  : fMeth(meth),
    fText(text)
{}

//------------------------------------------+-----------------------------------
//! Construct from method and message text and errno

RerrMsg::RerrMsg(const std::string& meth, const std::string& text, int errnum)
  : fMeth(meth),
    fText(text)
{
  AppendErrno(errnum);
}

//------------------------------------------+-----------------------------------
//! Destructor

RerrMsg::~RerrMsg()
{}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void RerrMsg::Init(const std::string& meth, const std::string& text)
{
  fMeth = meth;
  fText = text;
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void RerrMsg::InitErrno(const std::string& meth,
                        const std::string& text, int errnum)
{
  fMeth = meth;
  fText = text;
  AppendErrno(errnum);
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void RerrMsg::InitPrintf(const std::string& meth, const char* format, ...)
{
  fMeth = meth;

  char buf[1024];
  buf[0] = 0;
  
  va_list ap;
  va_start (ap, format);
  vsnprintf (buf, sizeof(buf), format, ap);
  va_end (ap);

  fText = buf;

  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void RerrMsg::Prepend(const std::string& meth)
{
  fMeth = meth + "->" + fMeth;
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void RerrMsg::Append(const std::string& text)
{
  fText += text;
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void RerrMsg::AppendErrno(int errnum)
{
  fText += strerror(errnum);
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void RerrMsg::AppendPrintf(const char* format, ...)
{
  char buf[1024];
  buf[0] = 0;
  
  va_list ap;
  va_start (ap, format);
  vsnprintf (buf, sizeof(buf), format, ap);
  va_end (ap);

  fText += buf;

  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

std::string RerrMsg::Message() const
{
  return fMeth + ": " + fText;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

void RerrMsg::Swap(RerrMsg& rhs)
{
  fMeth.swap(rhs.fMeth);
  fText.swap(rhs.fText);
  return;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

RerrMsg& RerrMsg::operator=(const RerrMsg& rhs)
{
  if (&rhs == this) return *this;
  fMeth = rhs.fMeth;
  fText = rhs.fText;
  return *this;  
}

} // end namespace CbmNet
