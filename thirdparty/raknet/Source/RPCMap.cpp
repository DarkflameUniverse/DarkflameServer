/// \file
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

#include "RPCMap.h"
#include <string.h>

RPCMap::RPCMap()
{
}
RPCMap::~RPCMap()
{
	Clear();
}
void RPCMap::Clear(void)
{
	unsigned i;
	RPCNode *node;
	for (i=0; i < rpcSet.Size(); i++)
	{
		node=rpcSet[i];
		if (node)
		{
			rakFree(node->uniqueIdentifier);
			delete node;
		}
	}
	rpcSet.Clear();
}
RPCNode *RPCMap::GetNodeFromIndex(RPCIndex index)
{
	if ((unsigned)index < rpcSet.Size())
		return rpcSet[(unsigned)index];
	return 0;
}
RPCNode *RPCMap::GetNodeFromFunctionName(const char *uniqueIdentifier)
{
	unsigned index;
	index=(unsigned)GetIndexFromFunctionName(uniqueIdentifier);
	if ((RPCIndex)index!=UNDEFINED_RPC_INDEX)
		return rpcSet[index];
	return 0;
}
RPCIndex RPCMap::GetIndexFromFunctionName(const char *uniqueIdentifier)
{
	unsigned index;
	for (index=0; index < rpcSet.Size(); index++)
		if (rpcSet[index] && strcmp(rpcSet[index]->uniqueIdentifier, uniqueIdentifier)==0)
			return (RPCIndex) index;
	return UNDEFINED_RPC_INDEX;
}

// Called from the user thread for the local system
void RPCMap::AddIdentifierWithFunction(const char *uniqueIdentifier, void *functionPointer, bool isPointerToMember)
{
#ifdef _DEBUG
	assert((int) rpcSet.Size()+1 < MAX_RPC_MAP_SIZE); // If this hits change the typedef of RPCIndex to use an unsigned short
	assert(uniqueIdentifier && uniqueIdentifier[0]);
	assert(functionPointer);
#endif

	unsigned index, existingNodeIndex;
	RPCNode *node;

	existingNodeIndex=GetIndexFromFunctionName(uniqueIdentifier);
	if ((RPCIndex)existingNodeIndex!=UNDEFINED_RPC_INDEX) // Insert at any free spot.
	{
		// Trying to insert an identifier at any free slot and that identifier already exists
		// The user should not insert nodes that already exist in the list
#ifdef _DEBUG
//		assert(0);
#endif
		return;
	}

	node = new RPCNode;
	node->uniqueIdentifier = (char*) rakMalloc( strlen(uniqueIdentifier)+1 );
	strcpy(node->uniqueIdentifier, uniqueIdentifier);
	node->functionPointer=functionPointer;
	node->isPointerToMember=isPointerToMember;

	// Insert into an empty spot if possible
	for (index=0; index < rpcSet.Size(); index++)
	{
		if (rpcSet[index]==0)
		{
			rpcSet.Replace(node, 0, index);
			return;
		}
	}

	rpcSet.Insert(node); // No empty spots available so just add to the end of the list

}
void RPCMap::AddIdentifierAtIndex(const char *uniqueIdentifier, RPCIndex insertionIndex)
{
#ifdef _DEBUG
	assert(uniqueIdentifier && uniqueIdentifier[0]);
#endif

	unsigned existingNodeIndex;
	RPCNode *node, *oldNode;

	existingNodeIndex=GetIndexFromFunctionName(uniqueIdentifier);

	if (existingNodeIndex==insertionIndex)
		return; // Already there

	if ((RPCIndex)existingNodeIndex!=UNDEFINED_RPC_INDEX)
	{
		// Delete the existing one
		oldNode=rpcSet[existingNodeIndex];
		rpcSet[existingNodeIndex]=0;
		rakFree(oldNode->uniqueIdentifier);
		delete oldNode;
	}

	node = new RPCNode;
	node->uniqueIdentifier = (char*) rakMalloc( strlen(uniqueIdentifier)+1 );
	strcpy(node->uniqueIdentifier, uniqueIdentifier);
	node->functionPointer=0;

	// Insert at a user specified spot
	if (insertionIndex < rpcSet.Size())
	{
		// Overwrite what is there already
		oldNode=rpcSet[insertionIndex];
		if (oldNode)
		{
			delete [] oldNode->uniqueIdentifier;
			delete oldNode;
		}
		rpcSet[insertionIndex]=node;
	}
	else
	{
		// Insert after the end of the list and use 0 as a filler for the empty spots
		rpcSet.Replace(node, 0, insertionIndex);
	}
}

void RPCMap::RemoveNode(const char *uniqueIdentifier)
{
	unsigned index;
	index=GetIndexFromFunctionName(uniqueIdentifier);
    #ifdef _DEBUG
	assert((int) index!=UNDEFINED_RPC_INDEX); // If this hits then the user was removing an RPC call that wasn't currently registered
	#endif
	RPCNode *node;
	node = rpcSet[index];
	rakFree(node->uniqueIdentifier);
	delete node;
	rpcSet[index]=0;
}

