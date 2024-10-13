#pragma once
#include <string>
#include <csignal>
#include <memory>

#include "TransportLayer.h"

enum class TransportType : uint32_t {
	RakNet,
	Tcp
};

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
		unsigned int zoneID = 0
	);

	~dServer();

	Packet* ReceiveFromMaster();
	
	Packet* Receive();

	void DeallocatePacket(Packet* packet);

	void DeallocateMasterPacket(Packet* packet);

	virtual void Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast);

	void SendToMaster(RakNet::BitStream& bitStream);

	void Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID);

	bool IsConnected(const SystemAddress& sysAddr);

	const std::string& GetIP() const;

	const int GetPort() const;

	const int GetMaxConnections() const;
	
	const bool GetIsEncrypted() const;

	const bool GetIsInternal() const;

	const bool GetIsOkay() const;

	Logger* GetLogger() const;

	const bool GetIsConnectedToMaster() const;

	const unsigned int GetZoneID() const;

	const int GetInstanceID() const;

	int GetPing(const SystemAddress& sysAddr) const;

	int GetLatestPing(const SystemAddress& sysAddr) const;

	const ServerType GetServerType() const;

	/**
	 * @brief Gets the transport layer.
	 * 
	 * @return The TransportLayer instance.
	 */
	const std::unique_ptr<TransportLayer>& GetTransportLayer() const;

	/**
	 * @brief Gets the transport type.
	 * 
	 * @return The transport type.
	 */
	const TransportType GetTransportType() const;

	/**
	 * Implicit conversion to TransportLayer*.
	 */
	operator TransportLayer*() const {
		return m_TransportLayer.get();
	}

	/**
	 * @brief Get pointer to the TransportLayer.
	 * 
	 * @return Pointer to the TransportLayer.
	 */
	TransportLayer* GetTransportLayerPtr() const {
		return m_TransportLayer.get();
	}

private:
	TransportType m_TransportType;

	std::unique_ptr<TransportLayer> m_TransportLayer;
};
