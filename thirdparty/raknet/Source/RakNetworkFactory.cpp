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

#include "LogCommandParser.h"
#include "RakNetworkFactory.h"
#include "RakPeerInterface.h"
#include "RakPeer.h"
#include "ConsoleServer.h"
#include "PacketLogger.h"
#include "RakNetCommandParser.h"
#include "ReplicaManager.h"
#include "RakNetTransport.h"
#include "TelnetTransport.h"
#include "PacketConsoleLogger.h"
#include "PacketFileLogger.h"
#include "Router.h"
#include "ConnectionGraph.h"

RakPeerInterface* RakNetworkFactory::GetRakPeerInterface( void )
{
	return new RakPeer;
}
ConsoleServer* RakNetworkFactory::GetConsoleServer( void )
{
	return new ConsoleServer;
}
ReplicaManager* RakNetworkFactory::GetReplicaManager( void )
{
	return new ReplicaManager;
}
LogCommandParser* RakNetworkFactory::GetLogCommandParser( void )
{
	return new LogCommandParser;
}
PacketLogger* RakNetworkFactory::GetPacketLogger( void )
{
	return new PacketLogger;
}
RakNetCommandParser* RakNetworkFactory::GetRakNetCommandParser( void )
{
	return new RakNetCommandParser;
}
RakNetTransport* RakNetworkFactory::GetRakNetTransport( void )
{
	return new RakNetTransport;
}
TelnetTransport* RakNetworkFactory::GetTelnetTransport( void )
{
	return new TelnetTransport;
}
PacketConsoleLogger* RakNetworkFactory::GetPacketConsoleLogger( void )
{
	return new PacketConsoleLogger;
}
PacketFileLogger* RakNetworkFactory::GetPacketFileLogger( void )
{
	return new PacketFileLogger;
}
Router* RakNetworkFactory::GetRouter( void )
{
	return new Router;
}
ConnectionGraph* RakNetworkFactory::GetConnectionGraph( void )
{
	return new ConnectionGraph;
}
void RakNetworkFactory::DestroyRakPeerInterface( RakPeerInterface* i )
{
	delete ( RakPeer* ) i;
}
void RakNetworkFactory::DestroyConsoleServer( ConsoleServer* i)
{
	delete ( ConsoleServer* ) i;
}
void RakNetworkFactory::DestroyReplicaManager( ReplicaManager* i)
{
	delete ( ReplicaManager* ) i;
}
void RakNetworkFactory::DestroyLogCommandParser( LogCommandParser* i)
{
	delete ( LogCommandParser* ) i;
}
void RakNetworkFactory::DestroyPacketLogger( PacketLogger* i)
{
	delete ( PacketLogger* ) i;
}
void RakNetworkFactory::DestroyRakNetCommandParser( RakNetCommandParser* i )
{
	delete ( RakNetCommandParser* ) i;
}
void RakNetworkFactory::DestroyRakNetTransport(  RakNetTransport* i )
{
	delete ( RakNetTransport* ) i;
}
void RakNetworkFactory::DestroyTelnetTransport(  TelnetTransport* i )
{
	delete ( TelnetTransport* ) i;
}
void RakNetworkFactory::DestroyPacketConsoleLogger(  PacketConsoleLogger* i )
{
	delete ( PacketConsoleLogger* ) i;
}
void RakNetworkFactory::DestroyPacketFileLogger(  PacketFileLogger* i )
{
	delete ( PacketFileLogger* ) i;
}
void RakNetworkFactory::DestroyRouter(  Router* i )
{
	delete ( Router* ) i;
}
void RakNetworkFactory::DestroyConnectionGraph(  ConnectionGraph* i )
{
	delete ( ConnectionGraph* ) i;
}
