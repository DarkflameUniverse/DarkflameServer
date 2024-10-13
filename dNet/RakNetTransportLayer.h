#pragma once

#include <string>
#include <csignal>
#include "RakPeerInterface.h"
#include "ReplicaManager.h"
#include "NetworkIDManager.h"

#include "TransportLayer.h"

class RakNetTransportLayer : public TransportLayer {
public:
	RakNetTransportLayer(
		const std::string& ip,
		int port,
		int instanceID,
		int maxConnections,
		bool isInternal,
		bool useEncryption,
		Logger* logger,
		const std::string managerIP,
		int managerPort,
		ServerType serverType,
		dConfig* config,
		Game::signal_t* shouldShutdown,
		unsigned int zoneID = 0
	);

	~RakNetTransportLayer();

	Packet* ReceiveFromManager() override;
	Packet* Receive() override;
	void DeallocatePacket(Packet* packet) override;
	void DeallocateManagerPacket(Packet* packet) override;
	void Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) override;
	void SendToManager(RakNet::BitStream& bitStream) override;

	void Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID) override;

	bool IsConnected(const SystemAddress& sysAddr) override;
	const std::string& GetIP() const override { return mIP; }
	const int GetPort() const override { return mPort; }
	const int GetMaxConnections() const override { return mMaxConnections; }
	const bool GetIsEncrypted() const override { return mUseEncryption; }
	const bool GetIsInternal() const override { return mIsInternal; }
	const bool GetIsOkay() const override { return mIsOkay; }
	Logger* GetLogger() const override { return mLogger; }
	const bool GetIsConnectedToManager() const override { return mManagerConnectionActive; }
	const unsigned int GetZoneID() const override { return mZoneID; }
	const int GetInstanceID() const override { return mInstanceID; }
	int GetPing(const SystemAddress& sysAddr) const override;
	int GetLatestPing(const SystemAddress& sysAddr) const override;

	const ServerType GetServerType() const override { return mServerType; }

	void UpdateBandwidthLimit();
	void UpdateMaximumMtuSize();

	TransportPeerInterface* CreateOutgoingTransport(
		uint32_t peerPort,
		const std::string& ip,
		uint32_t port,
		const std::string& password
	) const override;

	bool Startup() override;
	void Shutdown() override;

private:
	bool StartPeer();

	void SetupForManagerConnection();
	bool ConnectToManager();

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
	bool mManagerConnectionActive;
	ServerType mServerType;

	RakPeerInterface* mManagerPeer = nullptr;
	SocketDescriptor mManagerSocketDescriptor;
	SystemAddress mManagerSystemAddress;
	std::string mManagerIP;
	int mManagerPort;
};
