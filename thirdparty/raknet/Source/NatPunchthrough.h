/// \file
/// \brief Contains the NAT-punchthrough plugin
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

#ifndef __NAT_PUNCHTHROUGH_H
#define __NAT_PUNCHTHROUGH_H

#include "RakNetTypes.h"
#include "Export.h"
#include "PluginInterface.h"
#include "PacketPriority.h"
#include "DS_List.h"

class RakPeerInterface;
struct Packet;

/// \defgroup NAT_PUNCHTHROUGH_GROUP NatPunchthrough
/// \ingroup PLUGINS_GROUP

/// \ingroup NAT_PUNCHTHROUGH_GROUP
/// \brief Prints logging output for NatPunchthrough
class RAK_DLL_EXPORT NatPunchthroughLogger
{
public:
	NatPunchthroughLogger() {}
	virtual ~NatPunchthroughLogger() {}
	virtual void OnMessage(const char *str);
};

/// \brief The NatPunchthrough class implements the NAT punch through technique, allowing two systems to connect to each other that are both behind NATs.
///
/// A NAT (Network Address Translator) is a system that will makes it so your system's IP address is different from the IP address exposed to the internet.
/// This provides some security and allows multiple computers, each with a different IP address, to share one IP address as seen by the internet.
///
/// The problem is that NATs also ignore packets sent to them unless they sent a packet to the sender first.
/// If two systems are both behind NATs, then neither system can connect to each other.
/// Furthermore, some NATs will impose a temporary ban on an IP address that send unsolicited packets to them.
///
/// This can be solved by using a third system, a facilitator, that is not behind a NAT and that both systems are already connected to.
/// It will synchronize a send between both NAT systems such that the routers will both consider themselves as handling a reply to a message, when in
/// fact they are handing an initial message.  As replies are allowed, both systems get their corresponding messages and the connection takes place.
/// S = system that wants to connect
/// F = facilitator
/// R = system to get the connection request.
///
/// S knows IP of R in advance.
/// 1. S->F facilitate connection request to R
/// 2. if (R is not is connected) F->S ID_NAT_TARGET_NOT_CONNECTED.  Exit.
/// 3. F -> (Ping S, Ping R), every X ms Y times.  Wait Max(Ping(s), Ping(r) * multiple ms more, then go to step 4.
/// 4. F picks time highest(ave of Ping R,S) * N from now and sends this time RELIABLE and timestamped to R,S.
/// 5. At time picked in (4), S attempts to connect to R.  R sends offline ping to S.
/// 6. If R disconnects before or at step 4, tell this to S via ID_NAT_TARGET_CONNECTION_LOST
///
/// \note Timing is important with this plugin.  You need to call RakPeer::Receive frequently.
/// \ingroup NAT_PUNCHTHROUGH_GROUP
class RAK_DLL_EXPORT NatPunchthrough : public PluginInterface
{
public:
	/// Constructor
	NatPunchthrough();

	/// Destructor
	virtual ~NatPunchthrough();

	/// Call with true to allow other systems to use this system as a NAT punch through facilitator.  This takes a little bandwidth but
	/// otherwise there is no reason to disallow it.
	/// Defaults to true
	/// \param[in] allow True to allow, false to disallow.
	void FacilitateConnections(bool allow);

	/// Call this to start to connect to the specified host (ip or domain name) and server port using \a facilitator to punch through a NAT
	/// This is a non-blocking operation
	/// You know the connection is successful when you get the message ID_CONNECTION_ACCEPTED.
	/// You know the connection failed when you get the message ID_CONNECTION_ATTEMPT_FAILED, ID_CONNECTION_BANNED, or ID_NAT_TARGET_NOT_CONNECTED
	/// Both you and the host must be connected to the facilitator.
	/// \pre Requires that you first call Initialize
	/// \pre Both \a host and this system must already be connected to the system at the address \a facilitator and facilitator must be running NatPunchthrough with FacilitateConnections(true) previously called.
	/// \param[in] destination Either a dotted IP address or a domain name of the system you ultimately want to connect to.
	/// \param[in] remotePort Which port to connect to of the system you ultimately want to connect to.
	/// \param[in] passwordData A data block that must match the data block on the \a host.  This can be just a password, or can be a stream of data
	/// \param[in] passwordDataLength The length in bytes of passwordData
	/// \return If you are not connected to the facilitator this function returns false.  Otherwise it returns true.
	bool Connect(const char* destination, unsigned short remotePort, const char *passwordData, int passwordDataLength, SystemAddress facilitator);

	/// Same as above, but takes a SystemAddress for a host
	/// \param[in] destination The address of the host to connect to.
	/// \param[in] remotePort Which port to connect to of the system you ultimately want to connect to.
	/// \param[in] passwordData A data block that must match the data block on the \a host.  This can be just a password, or can be a stream of data
	/// \param[in] passwordDataLength The length in bytes of passwordData
	/// \return If you are not connected to the facilitator this function returns false.  Otherwise it returns true.
	bool Connect(SystemAddress destination, const char *passwordData, int passwordDataLength, SystemAddress facilitator);

	/// Free internal memory.
	void Clear(void);

	/// Sets the log output to print messages to
	void SetLogger(NatPunchthroughLogger *l);

	/// \internal For plugin handling
	virtual void OnAttach(RakPeerInterface *peer);
	/// \internal For plugin handling
	virtual void Update(RakPeerInterface *peer);
	/// \internal For plugin handling
	virtual PluginReceiveResult OnReceive(RakPeerInterface *peer, Packet *packet);
	/// \internal For plugin handling
	virtual void OnShutdown(RakPeerInterface *peer);
	/// \internal For plugin handling
	virtual void OnCloseConnection(RakPeerInterface *peer, SystemAddress systemAddress);

	struct ConnectionRequest
	{
		// Used by all
		// 0 means unset.  non-zero means send a connection request or offline message at that time.
		// If the sender is set, then we send an offline message.  If the reciever is set, we send a connection request.
		RakNetTime nextActionTime;

		// Used by sender and facilitator
		bool facilitatingConnection;
		SystemAddress receiverPublic;
		SystemAddress receiverPrivate;
		// Used to remove old connection Requests
		RakNetTime timeoutTime;

		// Used only by sender
		SystemAddress facilitator;
		char* passwordData;
		int passwordDataLength;
		// 0 for unset. Used to store the receiver's port if they can send to us but we (the sender) can't send to them.
		// If this is set, we abandon the normal series of guesses and just use this directly since we now know it's open and valid.
		unsigned short advertisedPort;

		// Used only by facilitator
		SystemAddress senderPublic;
		SystemAddress senderPrivate;
		unsigned char pingCount;

		// Used by recipient
		int recipientOfflineCount;

		bool attemptedConnection;

		void GetAddressList(RakPeerInterface *rakPeer, DataStructures::List<SystemAddress> &fallbackAddresses, SystemAddress publicAddress, SystemAddress privateAddress, bool excludeConnected);
	};
protected:
	void OnPunchthroughRequest(RakPeerInterface *peer, Packet *packet);
	void OnNATAdvertiseRecipientPort(RakPeerInterface *peer, Packet *packet);
	void OnConnectAtTime(RakPeerInterface *peer, Packet *packet);
	void OnSendOfflineMessageAtTime(RakPeerInterface *peer, Packet *packet);
	void RemoveRequestByFacilitator(SystemAddress systemAddress);
	void LogOut(const char *l);
	PluginReceiveResult OnConnectionAttemptFailed(Packet *packet);
	PluginReceiveResult OnNewIncomingConnection(Packet *packet);
	PluginReceiveResult OnConnectionRequestAccepted(Packet *packet);

	bool allowFacilitation;
	RakPeerInterface *rakPeer;
	DataStructures::List<ConnectionRequest*> connectionRequestList;
	NatPunchthroughLogger *log;
};

#endif
