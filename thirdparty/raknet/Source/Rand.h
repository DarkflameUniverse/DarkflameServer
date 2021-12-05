/// \file
/// \brief \b [Internal] Random number generator
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


#ifndef __RAND_H
#define __RAND_H 

#include "Export.h"

 /// Initialise seed for Random Generator
 /// \param[in] seed The seed value for the random number generator.
extern void RAK_DLL_EXPORT seedMT( unsigned int seed );

/// \internal
extern unsigned int RAK_DLL_EXPORT reloadMT( void );

/// Gets a random unsigned int
/// \return an integer random value.
extern unsigned int RAK_DLL_EXPORT randomMT( void );

/// Gets a random float
/// \return 0 to 1.0f, inclusive
extern float RAK_DLL_EXPORT frandomMT( void );

#endif
