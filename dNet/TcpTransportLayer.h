#pragma once

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <memory>
#include <csignal>
#include <unordered_map>
#include <functional>
#include <boost/functional/hash.hpp>

#include "RakPeerInterface.h"
#include "ReplicaManager.h"
#include "NetworkIDManager.h"

#include "TransportLayer.h"
#include "TransportPeerInterface.h"
#include "TcpSession.h"

namespace std {
	// SystemAddress hash specialization
	template <>
	struct hash<SystemAddress> {
		std::size_t operator()(const SystemAddress& sysAddr) const {
			const std::size_t hash1 = sysAddr.binaryAddress;
			const std::size_t hash2 = sysAddr.port;
			return (hash1 | (hash2 << 32));
		}
	};
}

class TcpPeer;

class TcpTransportLayer : public TransportLayer {
public:
	TcpTransportLayer(
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

	~TcpTransportLayer();

	Packet* ReceiveFromManager() override;
	
	Packet* Receive() override;

	void DeallocatePacket(Packet* packet) override;

	void DeallocateManagerPacket(Packet* packet) override;

	void Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) override;
	
	void SendToManager(RakNet::BitStream& bitStream) override;

	void Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID) override;

	bool IsConnected(const SystemAddress& sysAddr) override;

	const std::string& GetIP() const override { return m_IP; }

	const int GetPort() const override { return m_Port; }

	const int GetMaxConnections() const override { return m_MaxConnections; }

	const bool GetIsEncrypted() const override { return m_UseTls; }

	const bool GetIsInternal() const override { return m_IsInternal; }

	const bool GetIsOkay() const override { return m_IsOkay; }

	Logger* GetLogger() const override { return m_Logger; }

	const bool GetIsConnectedToManager() const override { return m_ManagerConnectionActive; }

	const unsigned int GetZoneID() const override { return m_ZoneID; }

	const int GetInstanceID() const override { return m_InstanceID; }

	int GetPing(const SystemAddress& sysAddr) const override;

	int GetLatestPing(const SystemAddress& sysAddr) const override;

	const ServerType GetServerType() const override { return m_ServerType; }

	TransportPeerInterface* CreateOutgoingTransport(
		uint32_t peerPort,
		const std::string& ip,
		uint32_t port,
		const std::string& password
	) const override;
	
	bool Startup() override;

	void Shutdown() override;

	void OnReceive(Packet* packet);

	void RemoveSession(const SystemAddress& sysAddr);

	uint32_t ClaimSessionID();

private:
	void SetupForManagerConnection();

	bool ConnectToManager();

private:
    void StartAccept();

    void HandleAccept(const std::shared_ptr<TcpSession>& session, const boost::system::error_code& ec);

    boost::asio::io_service m_IOService;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_Acceptor;
    std::unique_ptr<boost::asio::ssl::context> m_SSLContext;
    bool m_UseTls;
    bool m_VerifyCertificate;

	std::unordered_map<SystemAddress, std::shared_ptr<TcpSession>> m_ClientSessions;
    std::queue<Packet*> m_ReceiveQueue;
    std::mutex m_Mutex;
    std::thread m_IOThread;

	Logger* m_Logger = nullptr;
	dConfig* m_Config = nullptr;

	Game::signal_t* m_ShouldShutdown = nullptr;
	
	std::string m_IP;
	int32_t m_Port;
	int32_t m_MaxConnections;
	uint32_t m_ZoneID;
	int32_t m_InstanceID;
	bool m_IsInternal;
	bool m_IsOkay;
	bool m_ManagerConnectionActive;
	ServerType m_ServerType;
	int32_t m_SessionID = 0;

	TcpPeer* m_ManagerTransport = nullptr;
	SystemAddress m_ManagerSystemAddress;
	std::string m_ManagerIP;
	int m_ManagerPort;
};
