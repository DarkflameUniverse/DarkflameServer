#define _VARIADIC_MAX 10
#include "dServer.h"
#include "dNetCommon.h"
#include "Logger.h"
#include "dConfig.h"

#include "RakNetworkFactory.h"
#include "MessageIdentifiers.h"
#include "eConnectionType.h"
#include "eServerMessageType.h"
#include "eManagerMessageType.h"

#include "BitStreamUtils.h"
#include "MasterPackets.h"
#include "ZoneInstanceManager.h"
#include "StringifiedEnum.h"

#include "RakNetTransportLayer.h"
#ifdef NET_ENABLE_TCP_TRANSPORT
#include "TcpTransportLayer.h"
#endif

dServer::dServer(
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
	unsigned int zoneID
) {
	m_TransportType = TransportType::RakNet;

	if (config->GetValue("transport_layer") == "tcp") {
		m_TransportType = TransportType::Tcp;
		LOG("Using TCP transport layer.");
	}
	else {
		LOG("Using RakNet transport layer.");
	}

	switch (m_TransportType) {
	case TransportType::RakNet:
		m_TransportLayer = std::make_unique<RakNetTransportLayer>(
			ip,
			port,
			instanceID,
			maxConnections,
			isInternal,
			useEncryption,
			logger,
			masterIP,
			masterPort,
			serverType,
			config,
			shouldShutdown,
			zoneID
		);
		break;
	case TransportType::Tcp:
#ifdef NET_ENABLE_TCP_TRANSPORT
		m_TransportLayer = std::make_unique<TcpTransportLayer>(
			ip,
			port,
			instanceID,
			maxConnections,
			isInternal,
			useEncryption,
			logger,
			masterIP,
			masterPort,
			serverType,
			config,
			shouldShutdown,
			zoneID
		);
#else
		throw std::runtime_error("TCP transport is not enabled!");
#endif
		break;
	}

	bool okey = m_TransportLayer->Startup();

	if (!okey) {
		LOG("Failed to start the server!");
		
		throw std::runtime_error("Failed to start the server!");
	}
}

dServer::~dServer() {
	m_TransportLayer->Shutdown();

	m_TransportLayer = nullptr;
}

Packet* dServer::ReceiveFromMaster() {
	return m_TransportLayer->ReceiveFromManager();
}

Packet* dServer::Receive() {
	return m_TransportLayer->Receive();
}

void dServer::DeallocatePacket(Packet* packet) {
	m_TransportLayer->DeallocatePacket(packet);
}

void dServer::DeallocateMasterPacket(Packet* packet) {
	m_TransportLayer->DeallocateManagerPacket(packet);
}

void dServer::Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) {
	m_TransportLayer->Send(bitStream, sysAddr, broadcast);
}

void dServer::SendToMaster(RakNet::BitStream& bitStream) {
	m_TransportLayer->SendToManager(bitStream);
}

void dServer::Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID) {
	m_TransportLayer->Disconnect(sysAddr, disconNotifyID);
}

bool dServer::IsConnected(const SystemAddress& sysAddr) {
	return m_TransportLayer->IsConnected(sysAddr);
}

int dServer::GetPing(const SystemAddress& sysAddr) const {
	return m_TransportLayer->GetPing(sysAddr);
}

int dServer::GetLatestPing(const SystemAddress& sysAddr) const {
	return m_TransportLayer->GetLatestPing(sysAddr);
}

const std::string& dServer::GetIP() const {
	return m_TransportLayer->GetIP();
}

const int dServer::GetPort() const {
	return m_TransportLayer->GetPort();
}

const int dServer::GetMaxConnections() const {
	return m_TransportLayer->GetMaxConnections();
}

const bool dServer::GetIsEncrypted() const {
	return m_TransportLayer->GetIsEncrypted();
}

const bool dServer::GetIsInternal() const {
	return m_TransportLayer->GetIsInternal();
}

const bool dServer::GetIsOkay() const {
	return m_TransportLayer->GetIsOkay();
}

Logger* dServer::GetLogger() const {
	return m_TransportLayer->GetLogger();
}

const bool dServer::GetIsConnectedToMaster() const {
	return m_TransportLayer->GetIsConnectedToManager();
}

const unsigned int dServer::GetZoneID() const {
	return m_TransportLayer->GetZoneID();
}

const int dServer::GetInstanceID() const {
	return m_TransportLayer->GetInstanceID();
}

const ServerType dServer::GetServerType() const {
	return m_TransportLayer->GetServerType();
}

const std::unique_ptr<TransportLayer>& dServer::GetTransportLayer() const {
	return m_TransportLayer;
}

const TransportType dServer::GetTransportType() const {
	return m_TransportType;
}
