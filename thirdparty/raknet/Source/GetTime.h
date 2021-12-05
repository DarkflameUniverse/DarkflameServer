/// \file
/// \brief Returns the value from QueryPerformanceCounter.  This is the function RakNet uses to represent time.
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

#ifndef __GET_TIME_H
#define __GET_TIME_H

#include "Export.h"
#include "RakNetTypes.h" // For RakNetTime

/// The namespace RakNet is not consistently used.  It's only purpose is to avoid compiler errors for classes whose names are very common.
/// For the most part I've tried to avoid this simply by using names very likely to be unique for my classes.
namespace RakNet
{
	/// Returns the value from QueryPerformanceCounter.  This is the function RakNet uses to represent time.
	RakNetTime RAK_DLL_EXPORT GetTime( void );
	RakNetTimeNS RAK_DLL_EXPORT GetTimeNS( void );
}

#endif
