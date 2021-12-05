/// \file
/// \brief \b Reference counted object. Very simple class for quick and dirty uses.
///
/// This file is part of RakNet Copyright 2003 Kevin Jenkins.
///
/// Usage of RakNet is subject to the appropriate license agreement.
/// Creative Commons Licensees are subject to the
/// license found at
/// http://creativecommons.org/licenses/by-nc/2.5/
/// Single application licensees are subject to the license found at
/// http://www.jenkinssoftware.com/SingleApplicationLicense.html
/// Custom license users are subject to the terms therein.
/// GPL license users are subject to the GNU General Public
/// License as published by the Free
/// Software Foundation; either version 2 of the License, or (at your
/// option) any later version.

#ifndef __REF_COUNTED_OBJ_H
#define __REF_COUNTED_OBJ_H

#include "RakMemoryOverride.h"

/// World's simplest class :)
class RefCountedObj : public RakNet::RakMemoryOverride
{
	public:
		RefCountedObj() {refCount=1;}
		virtual ~RefCountedObj() {}
		void AddRef(void) {refCount++;}
		void Deref(void) {if (--refCount==0) delete this;}
		int refCount;
};

#endif
