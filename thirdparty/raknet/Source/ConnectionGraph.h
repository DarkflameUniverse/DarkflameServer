/// \file
/// \brief Connection graph plugin.  This maintains a graph of connections for the entire network, so every peer knows about every other peer.
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

#ifndef __CONNECTION_GRAPH_H
#define __CONNECTION_GRAPH_H

class RakPeerInterface;
#include "RakMemoryOverride.h"
#include "RakNetTypes.h"
#include "PluginInterface.h"
#include "DS_List.h"
#include "DS_WeightedGraph.h"
#include "GetTime.h"
#include "Export.h"

// If you need more than 255 groups just change this typedef
typedef unsigned char ConnectionGraphGroupID;

/// \defgroup CONNECTION_GRAPH_GROUP ConnectionGraph
/// \ingroup PLUGINS_GROUP

/// \ingroup CONNECTION_GRAPH_GROUP
/// \brief A connection graph.  Each peer will know about all other peers.
class RAK_DLL_EXPORT ConnectionGraph : public PluginInterface
{
public:
	ConnectionGraph();
	virtual ~ConnectionGraph();

	/// A node in the connection graph
	struct RAK_DLL_EXPORT SystemAddressAndGroupId
	{
		SystemAddressAndGroupId();
		~SystemAddressAndGroupId();
		SystemAddressAndGroupId(SystemAddress _systemAddress, ConnectionGraphGroupID _groupID);

		SystemAddress systemAddress;
		ConnectionGraphGroupID groupId;

		bool operator==( const SystemAddressAndGroupId& right ) const;
		bool operator!=( const SystemAddressAndGroupId& right ) const;
		bool operator > ( const SystemAddressAndGroupId& right ) const;
		bool operator < ( const SystemAddressAndGroupId& right ) const;
	};

	// --------------------------------------------------------------------------------------------
	// User functions
	// --------------------------------------------------------------------------------------------
	/// Plaintext encoding of the password, or 0 for none.  If you use a password, use secure connections
	void SetPassword(const char *password);

	/// Returns the connection graph
	/// \return The connection graph, stored as map of adjacency lists
	DataStructures::WeightedGraph<ConnectionGraph::SystemAddressAndGroupId, unsigned short, false> *GetGraph(void);

	/// Automatically add new connections to the connection graph
	/// Defaults to true
	/// If true, then the system will automatically add all new connections for you, assigning groupId 0 to these systems.
	/// If you want more control, you should call SetAutoAddNewConnections(false);
	/// When false, it is up to you to call RequestConnectionGraph and AddNewConnection to complete the graph
	/// However, this way you can choose which nodes are on the graph for this system and can assign groupIds to those nodes
	/// \param[in] autoAdd true to automatically add new connections to the connection graph.  False to not do so.
	void SetAutoAddNewConnections(bool autoAdd);

	/// Requests the connection graph from another system
	/// Only necessary to call if SetAutoAddNewConnections(false) is called.
	/// You should call this sometime after getting ID_CONNECTION_REQUEST_ACCEPTED and \a systemAddress is or should be a node on the connection graph
	/// \param[in] peer The instance of RakPeer to send through
	/// \param[in] systemAddress The system to send to
	void RequestConnectionGraph(RakPeerInterface *peer, SystemAddress systemAddress);

	/// Adds a new connection to the connection graph from this system to the specified system.  Also assigns a group identifier for that system
	/// Only used and valid when SetAutoAddNewConnections(false) is called.
	/// Call this for this system sometime after getting ID_NEW_INCOMING_CONNECTION or ID_CONNECTION_REQUEST_ACCEPTED for systems that contain a connection graph
	/// Groups are sets of one or more nodes in the total graph
	/// We only add to the graph groups which we subscribe to
	/// \param[in] peer The instance of RakPeer to send through
	/// \param[in] systemAddress The system that is connected to us.
	/// \param[in] groupId Just a number representing a group.  Important: 0 is reserved to mean unassigned group ID and is assigned to all systems added with SetAutoAddNewConnections(true)
	void AddNewConnection(RakPeerInterface *peer, SystemAddress systemAddress, ConnectionGraphGroupID groupId);

	/// Sets our own group ID
	/// Only used and valid when SetAutoAddNewConnections(false) is called.
	/// Defaults to 0
	/// \param[in] groupId Our group ID
	void SetGroupId(ConnectionGraphGroupID groupId);

	/// Allows adding to the connection graph nodes with this groupId.
	/// By default, you subscribe to group 0, which are all systems automatically added with SetAutoAddNewConnections(true)
	/// Calling this does not add nodes which were previously rejected due to an unsubscribed group - it only allows addition of nodes after the fact
	/// \param[in] groupId Just a number representing a group.  0 is reserved to mean unassigned group ID, automatically added with SetAutoAddNewConnections(true)
	void SubscribeToGroup(ConnectionGraphGroupID groupId);

	/// Disables addition of graph nodes with this groupId
	/// Calling this does not add remove nodes with this groupId which are already present in the graph.  It only disables addition of nodes after the fact
	/// \param[in] groupId Just a number representing a group.  0 is reserved to mean unassigned group ID, automatically added with SetAutoAddNewConnections(true)
	void UnsubscribeFromGroup(ConnectionGraphGroupID groupId);

	// --------------------------------------------------------------------------------------------
	// Packet handling functions
	// --------------------------------------------------------------------------------------------
	/// \internal
	virtual void OnShutdown(RakPeerInterface *peer);
	/// \internal
	virtual void Update(RakPeerInterface *peer);
	/// \internal
	virtual PluginReceiveResult OnReceive(RakPeerInterface *peer, Packet *packet);
	/// \internal
	virtual void OnCloseConnection(RakPeerInterface *peer, SystemAddress systemAddress);

protected:
	void HandleDroppedConnection(RakPeerInterface *peer, SystemAddress systemAddress, unsigned char packetId);
	void DeleteFromPeerList(SystemAddress systemAddress);

	void OnNewIncomingConnection(RakPeerInterface *peer, Packet *packet);
	void OnConnectionRequestAccepted(RakPeerInterface *peer, Packet *packet);
	void OnConnectionGraphRequest(RakPeerInterface *peer, Packet *packet);
	void OnConnectionGraphReply(RakPeerInterface *peer, Packet *packet);
	void OnConnectionGraphUpdate(RakPeerInterface *peer, Packet *packet);
	void OnNewConnection(RakPeerInterface *peer, Packet *packet);
	bool OnConnectionLost(RakPeerInterface *peer, Packet *packet, unsigned char packetId);
	void OnConnectionAddition(RakPeerInterface *peer, Packet *packet);
	void OnConnectionRemoval(RakPeerInterface *peer, Packet *packet);
	void SendConnectionGraph(SystemAddress target, unsigned char packetId, RakPeerInterface *peer);
	void SerializeWeightedGraph(RakNet::BitStream *out, const DataStructures::WeightedGraph<ConnectionGraph::SystemAddressAndGroupId, unsigned short, false> &g) const;
	bool DeserializeWeightedGraph(RakNet::BitStream *inBitstream, RakPeerInterface *peer);
	void AddAndRelayConnection(DataStructures::OrderedList<SystemAddress,SystemAddress> &ignoreList, const SystemAddressAndGroupId &conn1, const SystemAddressAndGroupId &conn2, unsigned short ping, RakPeerInterface *peer);
	bool RemoveAndRelayConnection(DataStructures::OrderedList<SystemAddress,SystemAddress> &ignoreList, unsigned char packetId, const SystemAddress node1, const SystemAddress node2, RakPeerInterface *peer);
	void RemoveParticipant(SystemAddress systemAddress);
	void AddParticipant(SystemAddress systemAddress);
	void BroadcastGraphUpdate(DataStructures::OrderedList<SystemAddress,SystemAddress> &ignoreList, RakPeerInterface *peer);
	void NotifyUserOfRemoteConnection(const SystemAddressAndGroupId &conn1, const SystemAddressAndGroupId &conn2,unsigned short ping, RakPeerInterface *peer);
	bool IsNewRemoteConnection(const SystemAddressAndGroupId &conn1, const SystemAddressAndGroupId &conn2,RakPeerInterface *peer);
	bool DeserializeIgnoreList(DataStructures::OrderedList<SystemAddress,SystemAddress> &ignoreList, RakNet::BitStream *inBitstream );
	void SerializeIgnoreListAndBroadcast(RakNet::BitStream *outBitstream, DataStructures::OrderedList<SystemAddress,SystemAddress> &ignoreList, RakPeerInterface *peer);

	RakNetTime nextWeightUpdate;
	char *pw;
	DataStructures::OrderedList<SystemAddress, SystemAddress> participantList;

	DataStructures::WeightedGraph<ConnectionGraph::SystemAddressAndGroupId, unsigned short, false> graph;
	bool autoAddNewConnections;
	ConnectionGraphGroupID myGroupId;

	DataStructures::OrderedList<ConnectionGraphGroupID, ConnectionGraphGroupID> subscribedGroups;

	// Used to broadcast new connections after some time so the pings are correct
	//RakNetTime forceBroadcastTime;

};

#endif
