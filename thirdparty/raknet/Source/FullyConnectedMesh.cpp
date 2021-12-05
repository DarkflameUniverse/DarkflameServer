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

#include "FullyConnectedMesh.h"
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "ConnectionGraph.h"
#include "NatPunchthrough.h"
#include <string.h>
#include <assert.h>

#ifdef _MSC_VER
#pragma warning( push )
#endif

FullyConnectedMesh::FullyConnectedMesh()
{
	pw=0;
	natPunchthrough=0;
}

FullyConnectedMesh::~FullyConnectedMesh()
{
	if (pw)
		rakFree(pw);
}

void FullyConnectedMesh::Startup(const char *password, int _passwordLength)
{
	if (pw)
		rakFree(pw);
	if (password)
	{
		pw = (char*) rakMalloc( _passwordLength );
		memcpy(pw, password, _passwordLength);
		passwordLength=_passwordLength;
	}
	else
		pw=0;
	
}

void FullyConnectedMesh::ConnectWithNatPunchthrough(NatPunchthrough *np, SystemAddress _facilitator)
	{
	natPunchthrough=np;
	facilitator=_facilitator;
	}


void FullyConnectedMesh::OnShutdown(RakPeerInterface *peer)
{
	(void) peer;
}
void FullyConnectedMesh::Update(RakPeerInterface *peer)
{
	(void) peer;
}

PluginReceiveResult FullyConnectedMesh::OnReceive(RakPeerInterface *peer, Packet *packet)
{
	assert(packet);
	assert(peer);

	switch (packet->data[0])
	{
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // This comes from the connection graph plugin
		{
			RakNet::BitStream b(packet->data, packet->length, false);
			b.IgnoreBits(8);
			ConnectionGraphGroupID group1, group2;
			SystemAddress node1, node2;
			b.Read(node1);
			b.Read(group1);
			if (peer->IsConnected(node1,true)==false)
			{
				if (natPunchthrough)
					natPunchthrough->Connect(node1, pw, pw ? passwordLength : 0, facilitator);
				else
					peer->Connect(node1.ToString(false), node1.port, pw, pw ? passwordLength : 0);
			}				
			b.Read(node2);
			b.Read(group2);
			if (peer->IsConnected(node2,true)==false)
			{
				if (natPunchthrough)
					natPunchthrough->Connect(node2, pw, pw ? passwordLength : 0, facilitator);
				else
					peer->Connect(node2.ToString(false), node2.port, pw, pw ? passwordLength : 0);
			}
				
			break;
		}
	}

	return RR_CONTINUE_PROCESSING;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
