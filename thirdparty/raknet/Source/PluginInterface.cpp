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

#include "PluginInterface.h"

#ifdef _MSC_VER
#pragma warning( push )
#endif

PluginInterface::PluginInterface()
{

}
PluginInterface::~PluginInterface()
{

}
void PluginInterface::OnAttach(RakPeerInterface *peer)
{
	(void) peer;
}
void PluginInterface::OnDetach(RakPeerInterface *peer)
{
	(void) peer;
}
void PluginInterface::OnStartup(RakPeerInterface *peer)
{
	(void) peer;
}
void PluginInterface::Update(RakPeerInterface *peer)
{
	(void) peer;
}
PluginReceiveResult PluginInterface::OnReceive(RakPeerInterface *peer, Packet *packet)
{
	(void) peer;
	(void) packet;
	return RR_CONTINUE_PROCESSING;
}
void PluginInterface::OnShutdown(RakPeerInterface *peer)
{
	(void) peer;
}
void PluginInterface::OnCloseConnection(RakPeerInterface *peer, SystemAddress systemAddress)
{
	(void) peer;
	(void) systemAddress;
}
void PluginInterface::OnDirectSocketSend(const char *data, const BitSize_t bitsUsed, SystemAddress remoteSystemAddress)
{
	(void) data;
	(void) bitsUsed;
	(void) remoteSystemAddress;
}
void PluginInterface::OnDirectSocketReceive(const char *data, const BitSize_t bitsUsed, SystemAddress remoteSystemAddress)
{
	(void) data;
	(void) bitsUsed;
	(void) remoteSystemAddress;
}
void PluginInterface::OnInternalPacket(InternalPacket *internalPacket, unsigned frameNumber, SystemAddress remoteSystemAddress, RakNetTime time, bool isSend)
{
	(void) internalPacket;
	(void) frameNumber;
	(void) remoteSystemAddress;
	(void) time;
	(void) isSend;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
