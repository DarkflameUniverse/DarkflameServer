#pragma once
#include <string>
#include "RakPeerInterface.h"
#include "ReplicaManager.h"
#include "NetworkIDManager.h"

class dLogger;

enum class ServerType : uint32_t {
	Master,
	Auth,
	Chat,
	World
};

class dServer {
public:
	dServer(const std::string& ip, int port, int instanceID, int maxConnections, bool isInternal, bool useEncryption, dLogger* logger, const std::string masterIP, int masterPort, ServerType serverType, unsigned int zoneID = 0);
	~dServer();

	Packet* ReceiveFromMaster();
	Packet* Receive();
	void DeallocatePacket(Packet* packet);
	void DeallocateMasterPacket(Packet* packet);
	void Send(RakNet::BitStream* bitStream, const SystemAddress& sysAddr, bool broadcast);
	void SendToMaster(RakNet::BitStream* bitStream);

	void Disconnect(const SystemAddress& sysAddr, uint32_t disconNotifyID);

	bool IsConnected(const SystemAddress& sysAddr);
	const std::string& GetIP() const { return mIP; }
	const int GetPort() const { return mPort; }
	const int GetMaxConnections() const { return mMaxConnections; }
	const bool GetIsEncrypted() const { return mUseEncryption; }
	const bool GetIsInternal() const { return mIsInternal; }
	const bool GetIsOkay() const { return mIsOkay; }
	dLogger* GetLogger() const { return mLogger; }
	const bool GetIsConnectedToMaster() const { return mMasterConnectionActive; }
	const unsigned int GetZoneID() const { return mZoneID; }
	const int GetInstanceID() const { return mInstanceID; }
	ReplicaManager* GetReplicaManager() { return mReplicaManager; }
	void UpdateReplica();

	int GetPing(const SystemAddress& sysAddr) const;
	int GetLatestPing(const SystemAddress& sysAddr) const;

	NetworkIDManager* GetNetworkIDManager() { return mNetIDManager; }

	const ServerType GetServerType() const { return mServerType; }

private:
	bool Startup();
	void Shutdown();
	void SetupForMasterConnection();
	bool ConnectToMaster();

private:
	dLogger* mLogger;
	RakPeerInterface* mPeer;
	ReplicaManager* mReplicaManager;
	NetworkIDManager* mNetIDManager;
	SocketDescriptor mSocketDescriptor;
	std::string mIP;
	int mPort;
	int mMaxConnections;
	unsigned int mZoneID;
	int mInstanceID;
	bool mUseEncryption;
	bool mIsInternal;
	bool mIsOkay;
	bool mMasterConnectionActive;
	ServerType mServerType;

	RakPeerInterface* mMasterPeer;
	SocketDescriptor mMasterSocketDescriptor;
	SystemAddress mMasterSystemAddress;
	std::string mMasterIP;
	int mMasterPort;
};
