/// \file
/// \brief \b [Internal] Holds information related to a RPC
///
/// \ingroup RAKNET_RPC
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

#ifndef __RPC_NODE
#define __RPC_NODE

#include "RakNetTypes.h"
#include "Export.h"

class RakPeerInterface;


/// \defgroup RAKNET_RPC Remote Procedure Call Subsystem 
/// \brief A system to call C or object member procudures on other systems, and even to return return values.

/// \ingroup RAKNET_RPC 
/// \internal
/// 
/// \brief Map registered procedure inside of a peer.  
/// 
struct RAK_DLL_EXPORT RPCNode
{
 
 	/// String identifier of the RPC
	char *uniqueIdentifier;
	
 /// Force casting of member functions to void *
	union
	{
		void ( *staticFunctionPointer ) ( RPCParameters *rpcParms );
		#if (defined(__GNUC__)  || defined(__GCCXML__))
  		void (*memberFunctionPointer)(void* _this, RPCParameters *rpcParms);
		#else
		void (__cdecl *memberFunctionPointer)(void* _this, RPCParameters *rpcParms);
		#endif

		void *functionPointer;
	};
	
	/// Is this a member function pointer?  True if so.  If false it's a regular C function.
	bool isPointerToMember;
};

#endif

