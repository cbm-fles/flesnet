// $Id: ReventFd.ipp 20 2013-08-10 16:42:54Z mueller $
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
// Imported Rev 488/475 1.0 from Retro -> CbmNet
// ---------------------------------------------------------------------------

/*!
  \file
  \version $Id: ReventFd.ipp 20 2013-08-10 16:42:54Z mueller $
  \brief   Implemenation (inline) of class ReventFd.
*/

// all method definitions in namespace CbmNet
namespace CbmNet {

//------------------------------------------+-----------------------------------
//! FIXME_docs

inline int ReventFd::Fd() const
{
  return fFd;
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

inline int ReventFd::Signal()
{
  return SignalFd(fFd);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

inline int ReventFd::Wait()
{
  return WaitFd(fFd);
}

//------------------------------------------+-----------------------------------
//! FIXME_docs

inline ReventFd::operator int() const
{
  return fFd;
}

} // end namespace CbmNet
