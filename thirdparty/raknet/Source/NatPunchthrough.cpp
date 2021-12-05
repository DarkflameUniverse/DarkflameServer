#include "NatPunchthrough.h"
#include "GetTime.h"
#include "RakPeerInterface.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"
#include "RakAssert.h"
#include "FormatString.h"

#include <stdio.h>
using namespace RakNet;

// Number is arbitrary, but you want a long enough interval to get a good sample, but short enough not to annoy the user.
static const int PING_INTERVAL=100;
// Fill up the ping buffer
static const int PING_COUNT=PING_TIMES_ARRAY_SIZE;
// What multiple to multiply the ping by to figure out how long to wait before sending the start time.
// Should be slightly larger than the ping so the last ping arrives
static const float SEND_TIMESTAMP_DELAY_PING_MULTIPLE=1.5f;
// What multiple to multiply the ping by to figure out when to send the NAT punchthrough request
// Should be large enough to make sure both systems get the message in the future, even if it needs to be resent due to packetloss.
// Too large is bad because the user has to wait and it makes it more likely our estimated ping is wrong
static const float SEND_PUNCHTHROUGH_DELAY_PING_MULTIPLE=3.0f;
static const int TTL_HOPS=3;

// As I add more out of band messages this will be moved elsewhere
static const char ID_NAT_ADVERTISE_RECIPIENT_PORT=0;

static const int RECIPIENT_OFFLINE_MAX_COUNT=10;
// See
// static const int MAX_OPEN_CONNECTION_REQUESTS=4;
// static const int TIME_BETWEEN_OPEN_CONNECTION_REQUESTS=500;
// In RakPeer.cpp
static const int RECIPIENT_OFFLINE_MESSAGE_INTERVAL=4000;

#ifdef _MSC_VER
#pragma warning( push )
#endif

void NatPunchthroughLogger::OnMessage(const char *str)
{
	printf("%s", str);
}

NatPunchthrough::NatPunchthrough()
{
	allowFacilitation=true;
	rakPeer=0;
	log=0;
}
NatPunchthrough::~NatPunchthrough()
{
	Clear();
}
void NatPunchthrough::FacilitateConnections(bool allow)
{
	allowFacilitation=allow;
}
bool NatPunchthrough::Connect(const char* destination, unsigned short remotePort, const char *passwordData, int passwordDataLength, SystemAddress facilitator)
{
	SystemAddress systemAddress;
	systemAddress.SetBinaryAddress(destination);
	systemAddress.port=remotePort;
	return Connect(systemAddress, passwordData, passwordDataLength, facilitator);
}
bool NatPunchthrough::Connect(SystemAddress destination, const char *passwordData, int passwordDataLength, SystemAddress facilitator)
{
	if (rakPeer->IsConnected(facilitator)==false)
		return false;

	RakNet::BitStream outBitstream;
	outBitstream.Write((MessageID)ID_NAT_PUNCHTHROUGH_REQUEST);
	// http://www.bford.info/pub/net/p2pnat/
	// A few poorly behaved NATs are known to scan the body of UDP datagrams for 4-byte fields that look like IP addresses, and translate them as they would the IP address fields in the IP header.
	outBitstream.Write(true);
	outBitstream.Write(destination);

	// Remember this connection request
	NatPunchthrough::ConnectionRequest *connectionRequest = new NatPunchthrough::ConnectionRequest;
	connectionRequest->receiverPublic=destination;
	connectionRequest->facilitator=facilitator;
	if (passwordDataLength)
	{
		connectionRequest->passwordData = (char*) rakMalloc( passwordDataLength );
		memcpy(connectionRequest->passwordData, passwordData, passwordDataLength);
	}
	else
		connectionRequest->passwordData = 0;
	connectionRequest->senderPublic=UNASSIGNED_SYSTEM_ADDRESS;
	connectionRequest->passwordDataLength=passwordDataLength;
	connectionRequest->facilitator=facilitator;
	connectionRequest->advertisedPort=0;
	connectionRequest->nextActionTime=0;
	connectionRequest->attemptedConnection=false;
	connectionRequest->facilitatingConnection=false;
	connectionRequest->timeoutTime=RakNet::GetTime()+30000;
	connectionRequest->pingCount=0;
	connectionRequestList.Insert(connectionRequest);

	rakPeer->Send(&outBitstream, HIGH_PRIORITY, RELIABLE, 0, facilitator, false);

	return true;
}
void NatPunchthrough::Clear(void)
{
	unsigned i;
	for (i=0; i < connectionRequestList.Size(); i++)
	{
		rakFree(connectionRequestList[i]->passwordData);
		delete connectionRequestList[i];
	}
	connectionRequestList.Clear();
}
void NatPunchthrough::OnAttach(RakPeerInterface *peer)
{
	rakPeer=peer;
}
void NatPunchthrough::Update(RakPeerInterface *peer)
{
	(void) peer;
	if (connectionRequestList.Size())
	{
		RakNetTime time = RakNet::GetTime();
		RakNet::BitStream outBitstream;
		unsigned i;
		i=0;
		while (i < connectionRequestList.Size())
		{
			// Remove old connection requests that get no answer
			if (connectionRequestList[i]->timeoutTime < time)
			{
				rakFree(connectionRequestList[i]->passwordData);
				delete connectionRequestList[i];
				connectionRequestList.RemoveAtIndex(i);
				continue;
			}
			
			// If we are a facilitator, do pings
			if (connectionRequestList[i]->facilitatingConnection)
			{
				if (time >= connectionRequestList[i]->nextActionTime )
				{
					if (connectionRequestList[i]->pingCount < PING_COUNT)
					{
						// Ping
						connectionRequestList[i]->pingCount++;
						rakPeer->Ping(connectionRequestList[i]->receiverPublic);
						rakPeer->Ping(connectionRequestList[i]->senderPublic);
						connectionRequestList[i]->nextActionTime=time+PING_INTERVAL;
					}
					else if (connectionRequestList[i]->pingCount == PING_COUNT)
					{
						// Done pinging.  Wait till the next stage
						int receiverPing, senderPing;
						receiverPing=rakPeer->GetAveragePing(connectionRequestList[i]->receiverPublic);
						senderPing=rakPeer->GetAveragePing(connectionRequestList[i]->senderPublic);
						if (receiverPing > senderPing)
							connectionRequestList[i]->nextActionTime=(RakNetTime)(receiverPing*SEND_TIMESTAMP_DELAY_PING_MULTIPLE);
						else
							connectionRequestList[i]->nextActionTime=(RakNetTime)(senderPing*SEND_TIMESTAMP_DELAY_PING_MULTIPLE);
						connectionRequestList[i]->pingCount++;
					}
					else
					{
						// Send the timestamped message to both systems so they send datagrams to each other simultaneously
						int receiverPing, senderPing;
						RakNetTime delayTime;
						receiverPing=rakPeer->GetAveragePing(connectionRequestList[i]->receiverPublic);
						senderPing=rakPeer->GetAveragePing(connectionRequestList[i]->senderPublic);
						if (receiverPing > senderPing)
							delayTime=(RakNetTime)(receiverPing*SEND_PUNCHTHROUGH_DELAY_PING_MULTIPLE);
						else
							delayTime=(RakNetTime)(senderPing*SEND_PUNCHTHROUGH_DELAY_PING_MULTIPLE);

						if (delayTime < 500)
							delayTime=500;

						outBitstream.Reset();
						outBitstream.Write((MessageID)ID_TIMESTAMP);
						outBitstream.Write(time+delayTime);
						outBitstream.Write((MessageID)ID_NAT_CONNECT_AT_TIME);
						// A few poorly behaved NATs are known to scan the body of UDP datagrams for 4-byte fields that look like IP addresses, and translate them as they would the IP address fields in the IP header.
						outBitstream.Write(true);
						outBitstream.Write(rakPeer->GetInternalID(connectionRequestList[i]->receiverPublic));
						rakPeer->Send(&outBitstream, SYSTEM_PRIORITY, RELIABLE, 0, connectionRequestList[i]->senderPublic, false);

						outBitstream.Reset();
						outBitstream.Write((MessageID)ID_TIMESTAMP);
						outBitstream.Write(time+delayTime);
						outBitstream.Write((MessageID)ID_NAT_SEND_OFFLINE_MESSAGE_AT_TIME);
						// A few poorly behaved NATs are known to scan the body of UDP datagrams for 4-byte fields that look like IP addresses, and translate them as they would the IP address fields in the IP header.
						outBitstream.Write(true);
						outBitstream.Write(connectionRequestList[i]->senderPublic);
						outBitstream.Write(rakPeer->GetInternalID(connectionRequestList[i]->senderPublic));
						rakPeer->Send(&outBitstream, SYSTEM_PRIORITY, RELIABLE, 0, connectionRequestList[i]->receiverPublic, false);

						delete [] connectionRequestList[i]->passwordData;
						delete connectionRequestList[i];
						connectionRequestList.RemoveAtIndex(i);
						continue;
					}
				}	
			}
			// Else not a facilitator.  If nextActionTime is non-zero, connect or send the offline message
			else if (connectionRequestList[i]->nextActionTime && time >= connectionRequestList[i]->nextActionTime && connectionRequestList[i]->attemptedConnection==false)
			{
				ConnectionRequest *connectionRequest = connectionRequestList[i];
				DataStructures::List<SystemAddress> fallbackAddresses;
//				unsigned fallbackIndex;
				RakNet::BitStream oob;
				if (connectionRequest->receiverPublic!=UNASSIGNED_SYSTEM_ADDRESS)
				{
			//		fallbackAddresses.Clear();
			//		connectionRequest->GetAddressList(peer, fallbackAddresses, connectionRequest->senderPublic, connectionRequest->senderPrivate, true);
			//		for (unsigned fallbackIndex=0; fallbackIndex < fallbackAddresses.Size(); fallbackIndex++)
			//			peer->Ping(fallbackAddresses[fallbackIndex].ToString(false),fallbackAddresses[fallbackIndex].port,false);

					// Connect to this system.
					LogOut(FormatString("Connecting to public address %s\n", connectionRequest->receiverPublic.ToString(true)));
					rakPeer->Connect(connectionRequest->receiverPublic.ToString(false), connectionRequest->receiverPublic.port, connectionRequest->passwordData, connectionRequest->passwordDataLength);
					connectionRequest->attemptedConnection=true;

					/*
					if (connectionRequestList[i]->receiverPrivate==connectionRequestList[i]->receiverPublic)
					{
						// Otherwise done with this connection, as it's a normal connection attempt
						rakFree(connectionRequestList[i]->passwordData);
						delete connectionRequestList[i];
						connectionRequestList.RemoveAtIndex(i);
						continue;
					}
					*/
				}
				else
				{
					RakAssert(connectionRequestList[i]->senderPublic!=UNASSIGNED_SYSTEM_ADDRESS);

					// Send ID_NAT_ADVERTISE_RECIPIENT_PORT to all possible remote ports
					fallbackAddresses.Clear();
					connectionRequest->GetAddressList(peer, fallbackAddresses, connectionRequest->senderPublic, connectionRequest->senderPrivate, true);
					
					/// 04/13/08 I think if I send to all possible fallbacks in order it starts reusing external ports or else assigns them in a random way
					/// Sending 4X to each port in order did work however.
					/// Timing matches that of RakPeer's failure notices and retries
					if ((unsigned int) connectionRequestList[i]->recipientOfflineCount < fallbackAddresses.Size())
					{
						unsigned k;
						//for (k=0; k < 4; k++)
						for (k=0; k < (unsigned) connectionRequestList[i]->recipientOfflineCount && k < fallbackAddresses.Size(); k++)
						{
							oob.Write(ID_NAT_ADVERTISE_RECIPIENT_PORT);
							oob.Write(rakPeer->GetExternalID(connectionRequest->facilitator));
							// This is duplicated intentionally, since the first packet is often dropped
							peer->SendOutOfBand(fallbackAddresses[k].ToString(false),fallbackAddresses[k].port,ID_OUT_OF_BAND_INTERNAL,(const char*) oob.GetData(),oob.GetNumberOfBytesUsed(),0);
							peer->SendOutOfBand(fallbackAddresses[k].ToString(false),fallbackAddresses[k].port,ID_OUT_OF_BAND_INTERNAL,(const char*) oob.GetData(),oob.GetNumberOfBytesUsed(),0);
						}

						LogOut(FormatString("Recipient sending ID_OUT_OF_BAND_INTERNAL to %s\n", fallbackAddresses[connectionRequestList[i]->recipientOfflineCount].ToString(true)));
					}

					/*
					for (fallbackIndex=0; fallbackIndex < fallbackAddresses.Size(); fallbackIndex++)
					{
						// This follows a similar path to the sender connecting to us.
						// If this message arrives, it lets the sender know our external port.
						// The sender can then use this port instead of just going down the fallbackAddresses guessing.
						// This saves us if the recipient is impossible to connect to, but the sender is not.
						oob.Reset();
						oob.Write(ID_NAT_ADVERTISE_RECIPIENT_PORT);
						oob.Write(rakPeer->GetExternalID(connectionRequest->facilitator));
						peer->SendOutOfBand(fallbackAddresses[fallbackIndex].ToString(false),fallbackAddresses[fallbackIndex].port,ID_OUT_OF_BAND_INTERNAL,(const char*) oob.GetData(),oob.GetNumberOfBytesUsed(),0);

						// bool SendOutOfBand(const char *host, unsigned short remotePort, MessageID header, const char *data, BitSize_t dataLength, unsigned connectionSocketIndex=0 );
					}
					*/


//					LogOut(FormatString("Pinging %s\n", connectionRequest->senderPublic.ToString(true)));

				//	rakPeer->Connect(connectionRequestList[i]->senderPublic.ToString(false),connectionRequestList[i]->senderPublic.port,0,0,0);

					/*
					SystemAddress senderAddrWithFPort;
					SystemAddress senderAddrWithRPort;
					SystemAddress senderAddrWithIPort;
					senderAddrWithFPort=connectionRequestList[i]->senderPublic;
					senderAddrWithFPort.port=connectionRequestList[i]->facilitator.port;
					senderAddrWithRPort=connectionRequestList[i]->senderPublic;
					senderAddrWithRPort.port=rakPeer->GetExternalID(connectionRequestList[i]->facilitator).port;
					senderAddrWithIPort=connectionRequestList[i]->senderPublic;
					senderAddrWithIPort.port=rakPeer->GetInternalID().port;
					*/

				//	LogOut(FormatString("Recipient sending TTL %i hops to %s\n", TTL_HOPS, connectionRequestList[i]->senderPublic.ToString()));
				//	rakPeer->SendTTL(connectionRequestList[i]->senderPublic.ToString(false), connectionRequestList[i]->senderPublic.port, false);


					// Send offline message to this system, hopefully at the exact same time that system tries to connect to us.
//					rakPeer->Ping(connectionRequestList[i]->senderPublic.ToString(false), connectionRequestList[i]->senderPublic.port, false);
					/*
					rakPeer->Ping(senderAddrWithFPort.ToString(false), senderAddrWithFPort.port, false);
					rakPeer->Ping(senderAddrWithRPort.ToString(false), senderAddrWithRPort.port, false);
					rakPeer->Ping(senderAddrWithIPort.ToString(false), senderAddrWithIPort.port, false);
					*/

			//		if (connectionRequestList[i]->senderPrivate!=connectionRequestList[i]->senderPublic)
			//			rakPeer->Ping(connectionRequestList[i]->senderPrivate.ToString(false), connectionRequestList[i]->senderPrivate.port, false);

			//		LogOut(FormatString("Recipient sending TTL %i hops to %s\n", TTL_HOPS, connectionRequestList[i]->senderPublic.ToString(false)));
				//	peer->SendTTL(connectionRequestList[i]->senderPublic.ToString(false),connectionRequestList[i]->senderPublic.port, TTL_HOPS+connectionRequestList[i]->recipientOfflineCount, 0);
				//	peer->SendTTL(connectionRequestList[i]->senderPublic.ToString(false),connectionRequestList[i]->senderPublic.port+1, TTL_HOPS+connectionRequestList[i]->recipientOfflineCount, 0);
				//	peer->SendTTL(connectionRequestList[i]->senderPrivate.ToString(false),connectionRequestList[i]->senderPrivate.port, TTL_HOPS+connectionRequestList[i]->recipientOfflineCount, 0);

					connectionRequestList[i]->recipientOfflineCount++;
					if (connectionRequestList[i]->recipientOfflineCount==RECIPIENT_OFFLINE_MAX_COUNT)
					{
						rakFree(connectionRequestList[i]->passwordData);
						delete connectionRequestList[i];
						connectionRequestList.RemoveAtIndex(i);
					}
					else
					{
						connectionRequestList[i]->nextActionTime=time+RECIPIENT_OFFLINE_MESSAGE_INTERVAL;
					}

					continue;
				}
			}
			
			i++;
		}
	}
}
PluginReceiveResult NatPunchthrough::OnReceive(RakPeerInterface *peer, Packet *packet)
{
	unsigned char packetId;
	if (packet->length>sizeof(MessageID)+sizeof(RakNetTime) && packet->data[0]==ID_TIMESTAMP)
		packetId=packet->data[sizeof(MessageID)+sizeof(RakNetTime)];
	else
		packetId=packet->data[0];

	switch (packetId) 
	{
	case ID_OUT_OF_BAND_INTERNAL:
		if (packet->length>=2 && packet->data[1]==ID_NAT_ADVERTISE_RECIPIENT_PORT)
		{
			OnNATAdvertiseRecipientPort(peer, packet);
			return RR_STOP_PROCESSING_AND_DEALLOCATE; // Absorb
		}
		break;
	case ID_NAT_PUNCHTHROUGH_REQUEST:
		OnPunchthroughRequest(peer, packet);
		return RR_STOP_PROCESSING_AND_DEALLOCATE; // Absorb
	case ID_NAT_TARGET_CONNECTION_LOST:
		RemoveRequestByFacilitator(packet->systemAddress);
		return RR_STOP_PROCESSING_AND_DEALLOCATE;
	case ID_NAT_TARGET_NOT_CONNECTED:
		RemoveRequestByFacilitator(packet->systemAddress);
		return RR_CONTINUE_PROCESSING;
	case ID_CONNECTION_ATTEMPT_FAILED:
		return OnConnectionAttemptFailed(packet);
	case ID_CONNECTION_REQUEST_ACCEPTED:
		return OnConnectionRequestAccepted(packet);
	case ID_NEW_INCOMING_CONNECTION:
		return OnNewIncomingConnection(packet);
	case ID_NAT_CONNECT_AT_TIME:
		OnConnectAtTime(peer, packet);
		return RR_STOP_PROCESSING_AND_DEALLOCATE; // Absorb
	case ID_NAT_SEND_OFFLINE_MESSAGE_AT_TIME:
		OnSendOfflineMessageAtTime(peer, packet);
		return RR_STOP_PROCESSING_AND_DEALLOCATE; // Absorb
	case ID_DISCONNECTION_NOTIFICATION:
	case ID_CONNECTION_LOST:
		OnCloseConnection(peer, packet->systemAddress);
		break;
	}

	return RR_CONTINUE_PROCESSING;
}
void NatPunchthrough::OnCloseConnection(RakPeerInterface *peer, SystemAddress systemAddress)
{
	(void) peer;
	if (allowFacilitation==false)
		return;

	// If in ping mode, send ID_NAT_TARGET_CONNECTION_LOST
	if (connectionRequestList.Size())
	{
		unsigned i;
		i=0;
		while (i < connectionRequestList.Size())
		{
			if (connectionRequestList[i]->facilitatingConnection &&
				(connectionRequestList[i]->receiverPublic==systemAddress ||
				connectionRequestList[i]->senderPublic==systemAddress))
			{
				LogOut(FormatString("Facilitator: Lost connection to %s\n", systemAddress.ToString(true)));

				// This field is not used by the facilitator.
				RakAssert(connectionRequestList[i]->passwordData==0);

				if (connectionRequestList[i]->senderPublic==systemAddress)
				{
					RakNet::BitStream outBitstream;
					outBitstream.Write((MessageID)ID_NAT_TARGET_CONNECTION_LOST);
					outBitstream.Write(connectionRequestList[i]->receiverPublic);
					rakPeer->Send(&outBitstream, HIGH_PRIORITY, RELIABLE, 0, connectionRequestList[i]->senderPublic, false);
				}

				rakFree(connectionRequestList[i]->passwordData);
				delete connectionRequestList[i];
				connectionRequestList.RemoveAtIndex(i);
			}
			else
				i++;
		}
	}
}
void NatPunchthrough::OnShutdown(RakPeerInterface *peer)
{
	(void) peer;
}
void NatPunchthrough::RemoveRequestByFacilitator(SystemAddress systemAddress)
{
	unsigned i;
	i=0;
	while (i < connectionRequestList.Size())
	{
		if (connectionRequestList[i]->facilitator==systemAddress)
		{
			rakFree(connectionRequestList[i]->passwordData);
			delete connectionRequestList[i];
			connectionRequestList.RemoveAtIndex(i);
			break;
		}
		else
			i++;
	}
}
PluginReceiveResult NatPunchthrough::OnConnectionAttemptFailed(Packet *packet)
{
//	SystemAddress publicOne, publicTwo;
//	SystemAddress privateZero, privateOne, privateTwo;
//	SystemAddress receiverAddrWithFPort;
//	SystemAddress receiverAddrWithSPort;
//	SystemAddress receiverAddrWithIPort;

	
	DataStructures::List<SystemAddress> fallbackAddresses;

	unsigned i,addressIndex;
	i=0;
	while (i < connectionRequestList.Size())
	{
		// Advertised port has priority. If set, use that. If fails on that, fail totally.
		if (connectionRequestList[i]->advertisedPort!=0)
		{
			SystemAddress advertisedAddress;
			advertisedAddress.binaryAddress=connectionRequestList[i]->receiverPublic.binaryAddress;
			advertisedAddress.port=connectionRequestList[i]->advertisedPort;
			if (packet->systemAddress==advertisedAddress)
			{
				// Couldn't connect to the advertised port, meaning uni-directional communication. Total failure.
				// Shouldn't really ever hit this.
				LogOut(FormatString("Failed to connect to remotely transmitted port from %s.\nCommunication would only be unidirectional.\n", packet->systemAddress.ToString(true)));
				
				rakFree(connectionRequestList[i]->passwordData);
				delete connectionRequestList[i];
				packet->systemAddress=connectionRequestList[i]->receiverPublic;
				connectionRequestList.RemoveAtIndex(i);
				return RR_CONTINUE_PROCESSING;
			}
			else
			{
				LogOut(FormatString("Connecting to remotely advertised address %s\n", advertisedAddress.ToString(true)));
				rakPeer->Connect(advertisedAddress.ToString(false), advertisedAddress.port, connectionRequestList[i]->passwordData, connectionRequestList[i]->passwordDataLength);
				return RR_STOP_PROCESSING_AND_DEALLOCATE;
			}
		}

		connectionRequestList[i]->GetAddressList(rakPeer, fallbackAddresses, connectionRequestList[i]->receiverPublic, connectionRequestList[i]->receiverPrivate, false);
		addressIndex=fallbackAddresses.GetIndexOf(packet->systemAddress);

		if (addressIndex != (unsigned) -1)
		{
			if (addressIndex<fallbackAddresses.Size()-1)
			{
				LogOut(FormatString("Connecting to fallback address %s\n", fallbackAddresses[addressIndex+1].ToString(true)));
				rakPeer->Connect(fallbackAddresses[addressIndex+1].ToString(false), fallbackAddresses[addressIndex+1].port, connectionRequestList[i]->passwordData, connectionRequestList[i]->passwordDataLength);

				return RR_STOP_PROCESSING_AND_DEALLOCATE;
			}
			else
			{
				LogOut(FormatString("NAT punchthrough failed."));
				LogOut(FormatString("Last attempted address=%s\n", packet->systemAddress.ToString()));
				LogOut(FormatString("Address list:\n"));
				unsigned j;
				for (j=0; j < fallbackAddresses.Size(); j++)
					LogOut(FormatString("%i. %s\n", j+1, fallbackAddresses[j].ToString()));

				// Totally failed
				rakFree(connectionRequestList[i]->passwordData);
				delete connectionRequestList[i];

				// Both attempts failed, return the notification
				packet->systemAddress=connectionRequestList[i]->receiverPublic;
				connectionRequestList.RemoveAtIndex(i);
				return RR_CONTINUE_PROCESSING;
			}
		}

		i++;
	}

	// Unrelated, return notification
	return RR_CONTINUE_PROCESSING;
}
PluginReceiveResult NatPunchthrough::OnConnectionRequestAccepted(Packet *packet)
{
	unsigned i;
	i=0;
	while (i < connectionRequestList.Size())
	{
		DataStructures::List<SystemAddress> fallbackAddresses;
		connectionRequestList[i]->GetAddressList(rakPeer, fallbackAddresses, connectionRequestList[i]->receiverPublic, connectionRequestList[i]->receiverPrivate, false);

		if (fallbackAddresses.GetIndexOf(packet->systemAddress)!=(unsigned) -1)
		{
			rakFree(connectionRequestList[i]->passwordData);
			delete connectionRequestList[i];
			connectionRequestList.RemoveAtIndex(i);
			return RR_CONTINUE_PROCESSING;
		}

		i++;
	}

	// return to user
	return RR_CONTINUE_PROCESSING;
}
PluginReceiveResult NatPunchthrough::OnNewIncomingConnection(Packet *packet)
{
	unsigned i;
	i=0;
	while (i < connectionRequestList.Size())
	{
		DataStructures::List<SystemAddress> fallbackAddresses;
		connectionRequestList[i]->GetAddressList(rakPeer, fallbackAddresses, connectionRequestList[i]->senderPublic, connectionRequestList[i]->senderPrivate, false);
		if (fallbackAddresses.GetIndexOf(packet->systemAddress)!=(unsigned) -1)
		{
			rakFree(connectionRequestList[i]->passwordData);
			delete connectionRequestList[i];
			connectionRequestList.RemoveAtIndex(i);
			return RR_CONTINUE_PROCESSING;
		}

		i++;
	}

	// return to user
	return RR_CONTINUE_PROCESSING;
}
void NatPunchthrough::OnPunchthroughRequest(RakPeerInterface *peer, Packet *packet)
{
	(void) peer;

	if (allowFacilitation==false)
		return;

	SystemAddress targetPublic;
	RakNet::BitStream inBitstream(packet->data, packet->length, false);
	RakNet::BitStream outBitstream;
	inBitstream.IgnoreBits(8);
	inBitstream.IgnoreBits(1);
	if (inBitstream.Read(targetPublic)==false)
		return;
	if (rakPeer->IsConnected(targetPublic)==false)
	{
		LogOut("ID_NAT_TARGET_NOT_CONNECTED\n");
		LogOut(FormatString("targetPublic=%s", targetPublic.ToString(true)));
		LogOut("Connection list:\n");
		SystemAddress remoteSystems[16];
		unsigned short numberOfSystems=16;
		rakPeer->GetConnectionList(remoteSystems, &numberOfSystems);
		for (unsigned i=0; i < numberOfSystems; i++)
			LogOut(FormatString("%i. %s\n", i+1, remoteSystems[i].ToString(true)));

		outBitstream.Write((MessageID)ID_NAT_TARGET_NOT_CONNECTED);
		outBitstream.Write(targetPublic);
		rakPeer->Send(&outBitstream, HIGH_PRIORITY, RELIABLE, 0, packet->systemAddress, false);
	}
	else
	{
		unsigned i;
		for (i=0; i<connectionRequestList.Size();i++)
		{
			if (connectionRequestList[i]->receiverPublic==targetPublic &&
				connectionRequestList[i]->senderPublic==packet->systemAddress)
			{
				outBitstream.Write((MessageID)ID_NAT_IN_PROGRESS);
				outBitstream.Write(targetPublic);
				rakPeer->Send(&outBitstream, HIGH_PRIORITY, RELIABLE, 0, packet->systemAddress, false);
				return;
			}
		}


		// Remember this connection request
		NatPunchthrough::ConnectionRequest *connectionRequest = new NatPunchthrough::ConnectionRequest;
		connectionRequest->receiverPublic=targetPublic;
		connectionRequest->facilitator=UNASSIGNED_SYSTEM_ADDRESS;
		connectionRequest->passwordData = 0;
		connectionRequest->senderPublic=packet->systemAddress;
		connectionRequest->advertisedPort=0;
		connectionRequest->passwordDataLength=0;
		connectionRequest->attemptedConnection=false; // Not actually necessary, only used when facilitatingConnection==false
		connectionRequest->facilitatingConnection=true;
		connectionRequest->nextActionTime=RakNet::GetTime()+PING_INTERVAL;
		connectionRequest->pingCount=1;
		connectionRequest->timeoutTime=RakNet::GetTime()+30000;
		connectionRequestList.Insert(connectionRequest);

		rakPeer->Ping(connectionRequest->receiverPublic);
		rakPeer->Ping(connectionRequest->senderPublic);
	}
}
void NatPunchthrough::OnNATAdvertiseRecipientPort(RakPeerInterface *peer, Packet *packet)
{
	(void) peer;

	// Find this connection request and set the connection timer.
	RakNet::BitStream inBitstream(packet->data, packet->length, false);
	inBitstream.IgnoreBits(16);
	SystemAddress recipientPublic;
	inBitstream.Read(recipientPublic);

	unsigned i;
	for (i=0; i < connectionRequestList.Size(); i++)
	{
		if (connectionRequestList[i]->receiverPublic==recipientPublic)
		{
			// Only write once, otherwise if it was different it would mess up OnConnectionAttemptFailed which only gives one try at a specific advertised port.
			if (connectionRequestList[i]->advertisedPort==0)
			{
				LogOut(FormatString("Got datagram from receiver with address %s\n", packet->systemAddress.ToString(true)));
				connectionRequestList[i]->advertisedPort=packet->systemAddress.port;
			}
			break;
		}
	}
}
void NatPunchthrough::OnConnectAtTime(RakPeerInterface *peer, Packet *packet)
{
	(void) peer;

	// Find this connection request and set the connection timer.
	unsigned i;
	for (i=0; i < connectionRequestList.Size(); i++)
	{
		if (connectionRequestList[i]->nextActionTime==0 &&
			connectionRequestList[i]->facilitatingConnection==false &&
			connectionRequestList[i]->facilitator==packet->systemAddress)
		{
			// Send a message at the router to the remote system, to open up the firewall / NAT
			// LogOut(FormatString("Sender sending TTL %i hops to %s\n", TTL_HOPS, connectionRequestList[i]->receiverPublic.ToString(false)));
			//BroadcastTTL(peer, connectionRequestList[i]->receiverPublic.ToString(false));
			// peer->SendTTL(connectionRequestList[i]->receiverPublic.ToString(false), connectionRequestList[i]->receiverPublic.port, TTL_HOPS, 0);
			//peer->SendTTL(connectionRequestList[i]->receiverPublic.ToString(false), connectionRequestList[i]->receiverPublic.port+1, TTL_HOPS, 0);


			
			RakNet::BitStream inBitstream(packet->data, packet->length, false);
			inBitstream.IgnoreBits(8); // ID_TIMESTAMP
			inBitstream.Read(connectionRequestList[i]->nextActionTime);
			inBitstream.IgnoreBits(8); // ID_NAT_CONNECT_AT_TIME
			inBitstream.IgnoreBits(1);
			inBitstream.Read(connectionRequestList[i]->receiverPrivate);

	//		peer->SendTTL(connectionRequestList[i]->receiverPrivate.ToString(false), connectionRequestList[i]->receiverPrivate.port, TTL_HOPS, 0);
	
			// Open all possible sender ports, so if they send to us on said port when guessing via ID_NAT_ADVERTISE_RECIPIENT_PORT, we can get the incoming packet
			DataStructures::List<SystemAddress> fallbackAddresses;
			ConnectionRequest *connectionRequest = connectionRequestList[i];
			connectionRequest->GetAddressList(peer, fallbackAddresses, connectionRequest->receiverPublic, connectionRequest->receiverPrivate, true);
			unsigned fallbackIndex;
			for (fallbackIndex=0; fallbackIndex < fallbackAddresses.Size(); fallbackIndex++)
				rakPeer->SendTTL(fallbackAddresses[fallbackIndex].ToString(false), fallbackAddresses[fallbackIndex].port, TTL_HOPS);
			LogOut(FormatString("Sender sending TTL with %i hops on %i ports to %s\n", TTL_HOPS, fallbackAddresses.Size(), connectionRequest->receiverPublic.ToString(false)));
					
			return;
		}
	}
}
void NatPunchthrough::OnSendOfflineMessageAtTime(RakPeerInterface *peer, Packet *packet)
{
	(void) peer;

	RakNet::BitStream inBitstream(packet->data, packet->length, false);
	RakNetTime nextActionTime;	
	inBitstream.IgnoreBits(8); // ID_TIMESTAMP
	SystemAddress senderPublic, senderPrivate;
	inBitstream.Read(nextActionTime); // TIMESTAMP
	inBitstream.IgnoreBits(8); // ID_NAT_SEND_OFFLINE_MESSAGE_AT_TIME
	inBitstream.IgnoreBits(1); // 1 bit offset
	inBitstream.Read(senderPublic);
	if (inBitstream.Read(senderPrivate)==false)
		return;

	// Just use the TTL2 here
	NatPunchthrough::ConnectionRequest *connectionRequest = new NatPunchthrough::ConnectionRequest;
	connectionRequest->receiverPublic=UNASSIGNED_SYSTEM_ADDRESS;
	connectionRequest->receiverPrivate=UNASSIGNED_SYSTEM_ADDRESS;
	connectionRequest->facilitator=packet->systemAddress;
	connectionRequest->senderPublic=senderPublic;
	connectionRequest->senderPrivate=senderPrivate;
	connectionRequest->passwordData = 0;
	connectionRequest->passwordDataLength=0;
	connectionRequest->advertisedPort=0;
	connectionRequest->attemptedConnection=false;
	connectionRequest->facilitatingConnection=false;
	connectionRequest->nextActionTime=nextActionTime;
	connectionRequest->pingCount=0;
	connectionRequest->timeoutTime=RakNet::GetTime()+30000;
	connectionRequest->recipientOfflineCount=0;
	connectionRequestList.Insert(connectionRequest);

	// Send a message at the router to the remote system, to open up the firewall / NAT
//	LogOut(FormatString("Recipient sending TTL %i hops to %s\n", TTL_HOPS, senderPublic.ToString(false)));
	//BroadcastTTL(peer, connectionRequest->senderPublic.ToString(false));
//	peer->SendTTL(senderPublic.ToString(false),senderPublic.port, TTL_HOPS, 0);
//	peer->SendTTL(senderPublic.ToString(false),senderPublic.port+1, TTL_HOPS, 0);
//	peer->SendTTL(senderPrivate.ToString(false),senderPrivate.port, TTL_HOPS, 0);


	// Open all possible sender ports, so if they send to us on said port, the router doesn't ban them
	DataStructures::List<SystemAddress> fallbackAddresses;
	connectionRequest->GetAddressList(peer, fallbackAddresses, connectionRequest->senderPublic, connectionRequest->senderPrivate, true);
	unsigned fallbackIndex;
	for (fallbackIndex=0; fallbackIndex < fallbackAddresses.Size(); fallbackIndex++)
		rakPeer->SendTTL(fallbackAddresses[fallbackIndex].ToString(false), fallbackAddresses[fallbackIndex].port, TTL_HOPS);
	LogOut(FormatString("Recipient sending TTL with %i hops on %i ports to %s\n", TTL_HOPS, fallbackAddresses.Size(), senderPublic.ToString()));


	/*
	SystemAddress senderAddrWithFPort;
	SystemAddress senderAddrWithRPort;
	SystemAddress senderAddrWithIPort;
	senderAddrWithFPort=connectionRequest->senderPublic;
	senderAddrWithFPort.port=connectionRequest->facilitator.port;
	senderAddrWithRPort=connectionRequest->senderPublic;
	senderAddrWithRPort.port=rakPeer->GetExternalID(connectionRequest->facilitator).port;
	senderAddrWithIPort=connectionRequest->senderPublic;
	senderAddrWithIPort.port=rakPeer->GetInternalID().port;
	*/
	// Send offline message to this system, hopefully at the exact same time that system tries to connect to us.
	/*
	rakPeer->SendTTL(senderAddrWithFPort.ToString(false), senderAddrWithFPort.port, false);
	rakPeer->SendTTL(senderAddrWithRPort.ToString(false), senderAddrWithRPort.port, false);
	rakPeer->SendTTL(senderAddrWithIPort.ToString(false), senderAddrWithIPort.port, false);
	*/
}
void NatPunchthrough::LogOut(const char *l)
{
	if (log)
		log->OnMessage(l);
}
void NatPunchthrough::SetLogger(NatPunchthroughLogger *l)
{
	log=l;
}
void NatPunchthrough::ConnectionRequest::GetAddressList(RakPeerInterface *rakPeer, DataStructures::List<SystemAddress> &fallbackAddresses, SystemAddress publicAddress, SystemAddress privateAddress, bool excludeConnected)
{
	SystemAddress fallback;
	fallbackAddresses.Clear(true);
	fallback.binaryAddress=publicAddress.binaryAddress;

	fallback.port=publicAddress.port;
	if (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false)
		fallbackAddresses.Insert(fallback);

	if (privateAddress!=publicAddress && privateAddress!=UNASSIGNED_SYSTEM_ADDRESS && (excludeConnected==false ||rakPeer->IsConnected(privateAddress,true)==false))
		fallbackAddresses.Insert(privateAddress);

	fallback.port=publicAddress.port+1;
	if (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false)
		fallbackAddresses.Insert(fallback);
	fallback.port=publicAddress.port+2;
	if (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false)
		fallbackAddresses.Insert(fallback);

	fallback.port=rakPeer->GetInternalID().port;
	if (fallbackAddresses.GetIndexOf(fallback)==(unsigned) -1 && (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false))
		fallbackAddresses.Insert(fallback);
	fallback.port=rakPeer->GetInternalID().port+1;
	if (fallbackAddresses.GetIndexOf(fallback)==(unsigned) -1 && (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false))
		fallbackAddresses.Insert(fallback);
	fallback.port=rakPeer->GetInternalID().port+2;
	if (fallbackAddresses.GetIndexOf(fallback)==(unsigned) -1 && (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false))
		fallbackAddresses.Insert(fallback);

	// Try to keep down the number of fallbacks or the router may run out of mappings
	/*
	fallback.port=publicAddress.port+3;
	if (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false)
		fallbackAddresses.Insert(fallback);
	fallback.port=publicAddress.port+4;
	if (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false)
		fallbackAddresses.Insert(fallback);

	fallback.port=rakPeer->GetInternalID().port+3;
	if (fallbackAddresses.GetIndexOf(fallback)==(unsigned) -1 && (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false))
		fallbackAddresses.Insert(fallback);
	fallback.port=rakPeer->GetInternalID().port+4;
	if (fallbackAddresses.GetIndexOf(fallback)==(unsigned) -1 && (excludeConnected==false || rakPeer->IsConnected(fallback,true)==false))
		fallbackAddresses.Insert(fallback);
		*/

}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
