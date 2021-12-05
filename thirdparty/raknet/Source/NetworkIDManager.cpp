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

#include "NetworkIDManager.h"
#include "NetworkIDObject.h"
#include "RakAssert.h"
#include <string.h> // For memset

bool NetworkIDNode::operator==( const NetworkIDNode& right ) const
{
	if ( networkID == right.networkID )
		return !0;

	return 0;
}

bool NetworkIDNode::operator > ( const NetworkIDNode& right ) const
{
	if ( networkID > right.networkID )
		return !0;

	return 0;
}

bool NetworkIDNode::operator < ( const NetworkIDNode& right ) const
{
	if ( networkID < right.networkID )
		return !0;

	return 0;
}

NetworkIDNode::NetworkIDNode()
{
	object = 0;
}

NetworkIDNode::NetworkIDNode( NetworkID _networkID, NetworkIDObject *_object )
{
	networkID = _networkID;
	object = _object;
}


//-------------------------------------------------------------------------------------
NetworkIDObject* NetworkIDManager::GET_BASE_OBJECT_FROM_ID( NetworkID x )
{
	if ( x == UNASSIGNED_NETWORK_ID )
		return 0;

#if defined(NETWORK_ID_USE_PTR_TABLE) || defined (NETWORK_ID_USE_HASH)
	// You can't use this technique in peer to peer mode.  Undefine NETWORK_ID_USE_PTR_TABLE in NetworkIDManager.h
	assert(NetworkID::peerToPeerMode==false);
	return IDArray[x.localSystemAddress];
#else

	NetworkIDNode *n = IDTree.GetPointerToNode( NetworkIDNode( ( x ), 0 ) );

	if ( n )
	{
		return n->object;
	}

	return 0;

#endif

}
//-------------------------------------------------------------------------------------
void* NetworkIDManager::GET_OBJECT_FROM_ID( NetworkID x )
{
#if defined(NETWORK_ID_USE_PTR_TABLE) || defined (NETWORK_ID_USE_HASH)
	if (x.localSystemAddress==65535)
		return 0;

	// You can't use this technique in peer to peer mode.  Undefine NETWORK_ID_USE_PTR_TABLE in NetworkIDManager.h
	assert(NetworkID::peerToPeerMode==false);
	if (IDArray[x.localSystemAddress])
	{
		if (IDArray[x.localSystemAddress]->GetParent())
		{
			return IDArray[x.localSystemAddress]->GetParent();
		}
		else
		{
#ifdef _DEBUG
			// If this assert hit then this object requires a call to SetParent and it never got one.
			assert(IDArray[x.localSystemAddress]->RequiresSetParent()==false);
#endif
			return IDArray[x.localSystemAddress];
		}
	}
#else
	NetworkIDObject *object = (NetworkIDObject *) GET_BASE_OBJECT_FROM_ID( x );
	if (object)
	{
		if (object->GetParent())
		{
			return object->GetParent();
		}
		else
		{
#ifdef _DEBUG
			// If this assert hit then this object requires a call to SetParent and it never got one.
			assert(object->RequiresSetParent()==false);
#endif
			return object;
		}
	}
#endif

	return 0;
}
//-------------------------------------------------------------------------------------
NetworkIDManager::NetworkIDManager(void)
{
	calledSetIsNetworkIDAuthority=false;
	sharedNetworkID=0;

#if defined(NETWORK_ID_USE_PTR_TABLE) || defined (NETWORK_ID_USE_HASH)
	// Last element is reserved for UNASSIGNED_NETWORK_ID
	IDArray = (NetworkIDObject**) rakMalloc(sizeof(NetworkIDObject*) * 65534);
	memset(IDArray,0,sizeof(NetworkIDObject*)*65534);
	// You can't use this technique in peer to peer mode.  Undefine NETWORK_ID_USE_PTR_TABLE in NetworkIDManager.h
	assert(NetworkID::peerToPeerMode==false);
#endif
}
//-------------------------------------------------------------------------------------
NetworkIDManager::~NetworkIDManager(void)
{
#if defined(NETWORK_ID_USE_PTR_TABLE) || defined (NETWORK_ID_USE_HASH)
	rakFree(IDArray);
#endif
}
//-------------------------------------------------------------------------------------

void NetworkIDManager::SetIsNetworkIDAuthority(bool isAuthority)
{
	isNetworkIDAuthority=isAuthority;
	calledSetIsNetworkIDAuthority=true;
}

//-------------------------------------------------------------------------------------

bool NetworkIDManager::IsNetworkIDAuthority(void) const
{
	RakAssert(calledSetIsNetworkIDAuthority);
	return isNetworkIDAuthority;
}

//-------------------------------------------------------------------------------------

unsigned short NetworkIDManager::GetSharedNetworkID( void )
{
	RakAssert(calledSetIsNetworkIDAuthority);
	return sharedNetworkID;
}

//-------------------------------------------------------------------------------------

void NetworkIDManager::SetSharedNetworkID( unsigned short i )
{
	RakAssert(calledSetIsNetworkIDAuthority);
	sharedNetworkID = i;
}

//-------------------------------------------------------------------------------------
void NetworkIDManager::SetExternalSystemAddress(SystemAddress systemAddress)
{
	RakAssert(calledSetIsNetworkIDAuthority);
	RakAssert(systemAddress!=UNASSIGNED_SYSTEM_ADDRESS);
	externalSystemAddress=systemAddress;
}
//-------------------------------------------------------------------------------------
SystemAddress NetworkIDManager::GetExternalSystemAddress(void)
{
	RakAssert(calledSetIsNetworkIDAuthority);
	return externalSystemAddress;
}
