#pragma once
#include <string>
#include <csignal>
#include "RakPeerInterface.h"
#include "ReplicaManager.h"
#include "NetworkIDManager.h"

class Logger;
class dConfig;
enum class eServerDisconnectIdentifiers : uint32_t;

enum class ServerType : uint32_t {
	Master,
	Auth,
	Chat,
	World
};

namespace Game {
	using signal_t = volatile std::sig_atomic_t;
}

class dServer {
public:
	// Default constructor should only used for testing!
	dServer() {};
	dServer(
		const std::string& ip,
		int port,
		int instanceID,
		int maxConnections,
		bool isInternal,
		bool useEncryption,
		Logger* logger,
		const std::string masterIP,
		int masterPort,
		ServerType serverType,
		dConfig* config,
		Game::signal_t* shouldShutdown,
		unsigned int zoneID = 0);
	~dServer();

	Packet* ReceiveFromMaster();
	Packet* Receive();
	void DeallocatePacket(Packet* packet);
	void DeallocateMasterPacket(Packet* packet);
	virtual void Send(RakNet::BitStream* bitStream, const SystemAddress& sysAddr, bool broadcast);
	void SendToMaster(RakNet::BitStream* bitStream);

	void Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID);

	bool IsConnected(const SystemAddress& sysAddr);
	const std::string& GetIP() const { return mIP; }
	const int GetPort() const { return mPort; }
	const int GetMaxConnections() const { return mMaxConnections; }
	const bool GetIsEncrypted() const { return mUseEncryption; }
	const bool GetIsInternal() const { return mIsInternal; }
	const bool GetIsOkay() const { return mIsOkay; }
	Logger* GetLogger() const { return mLogger; }
	const bool GetIsConnectedToMaster() const { return mMasterConnectionActive; }
	const unsigned int GetZoneID() const { return mZoneID; }
	const int GetInstanceID() const { return mInstanceID; }
	ReplicaManager* GetReplicaManager() { return mReplicaManager; }
	void UpdateReplica();
	void UpdateBandwidthLimit();
	void UpdateMaximumMtuSize();

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
	Logger* mLogger = nullptr;
	dConfig* mConfig = nullptr;
	RakPeerInterface* mPeer = nullptr;
	ReplicaManager* mReplicaManager = nullptr;
	NetworkIDManager* mNetIDManager = nullptr;

	/**
	 * Whether or not to shut down the server.  Pointer to Game::lastSignal.
	 */
	Game::signal_t* mShouldShutdown = nullptr;
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

	RakPeerInterface* mMasterPeer = nullptr;
	SocketDescriptor mMasterSocketDescriptor;
	SystemAddress mMasterSystemAddress;
	std::string mMasterIP;
	int mMasterPort;
};
