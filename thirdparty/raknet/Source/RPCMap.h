/// \file
/// \brief \b [Internal] A container class for a list of RPCNodes
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

#ifndef __RPC_MAP
#define __RPC_MAP

#include "RakMemoryOverride.h"
#include "RPCNode.h"
#include "DS_List.h"
#include "RakNetTypes.h"
#include "Export.h"

/// \ingroup RAKNET_RPC 
/// \internal
/// \brief A container class for a list of RPCNodes
struct RAK_DLL_EXPORT RPCMap : public RakNet::RakMemoryOverride
{
public:
	RPCMap();
	~RPCMap();
	void Clear(void);
    RPCNode *GetNodeFromIndex(RPCIndex index);
	RPCNode *GetNodeFromFunctionName(const char *uniqueIdentifier);
	RPCIndex GetIndexFromFunctionName(const char *uniqueIdentifier);
	void AddIdentifierWithFunction(const char *uniqueIdentifier, void *functionPointer, bool isPointerToMember);
	void AddIdentifierAtIndex(const char *uniqueIdentifier, RPCIndex insertionIndex);
	void RemoveNode(const char *uniqueIdentifier);
protected:
	DataStructures::List<RPCNode *> rpcSet;
};

#endif

