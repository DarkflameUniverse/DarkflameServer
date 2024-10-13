#define _VARIADIC_MAX 10

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
#include "TcpTransportLayer.h"
#include "TcpSession.h"
#include "TcpPeer.h"


TcpTransportLayer::TcpTransportLayer(
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
	unsigned int zoneID
) {
	m_IP = ip;
	m_Port = port;
	m_ZoneID = zoneID;
	m_InstanceID = instanceID;
	m_MaxConnections = maxConnections;
	m_IsInternal = isInternal;
	m_UseTls = config->GetValue("use_tls") == "1";
	m_Logger = logger;
	m_ManagerIP = managerIP;
	m_ManagerPort = managerPort;
	m_ManagerConnectionActive = false;
	m_ServerType = serverType;
	m_Config = config;
	m_ShouldShutdown = shouldShutdown;
	m_IsOkay = false;
	m_ManagerConnectionActive = false;
	m_ManagerTransport = nullptr;
	m_SessionID = 0;
	m_Acceptor = nullptr;
	m_SSLContext = nullptr;
}

TcpTransportLayer::~TcpTransportLayer() {

}

Packet* TcpTransportLayer::ReceiveFromManager() {
	if (m_ManagerTransport == nullptr) {
		throw std::runtime_error("Manager connection is not active!");
	}

	if (!m_ManagerConnectionActive) {
		throw std::runtime_error("Manager connection is not active!");
	}

	auto* packet = m_ManagerTransport->Receive();

	if (packet) {
		if (packet->length < 1) { m_ManagerTransport->DeallocatePacket(packet); return nullptr; }

		if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
			LOG("Lost our connection to Manager, shutting DOWN!");
			m_ManagerConnectionActive = false;
			//ConnectToManager(); //We'll just shut down now
		}

		if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) {
			LOG("Established connection to Manager, zone (%i), instance (%i)", this->GetZoneID(), this->GetInstanceID());
			m_ManagerConnectionActive = true;
			m_ManagerSystemAddress = packet->systemAddress;
			MasterPackets::SendServerInfo(this, packet);
		}

		if (packet->data[0] == ID_USER_PACKET_ENUM) {
			if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::MASTER) {
				switch (static_cast<eManagerMessageType>(packet->data[3])) {
				case eManagerMessageType::REQUEST_ZONE_TRANSFER_RESPONSE: {
					ZoneInstanceManager::Instance()->HandleRequestZoneTransferResponse(packet);
					break;
				}
				case eManagerMessageType::SHUTDOWN:
					*m_ShouldShutdown = -2;
					break;

				//When we handle these packets in World instead RakNetTransportLayer, we just return the packet's pointer.
				default:

					return packet;
				}
			}
		}

		m_ManagerTransport->DeallocatePacket(packet);
	}

	return nullptr;
}

Packet* TcpTransportLayer::Receive() {
    std::unique_lock<std::mutex> lock(m_Mutex);

	if (m_ReceiveQueue.empty()) {
		return nullptr;
	}

    Packet* packet = m_ReceiveQueue.front();
    m_ReceiveQueue.pop();

    return packet;
}

void TcpTransportLayer::DeallocatePacket(Packet* packet) {
	delete[] packet->data;
	delete packet;
}

void TcpTransportLayer::DeallocateManagerPacket(Packet* packet) {
	if (m_ManagerTransport) {
		m_ManagerTransport->DeallocatePacket(packet);
	}
}

void TcpTransportLayer::Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) {
	if (broadcast) {
		for (auto& [addr, session] : m_ClientSessions) {
			session->Send(bitStream);
		}

		return;
	}

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) {
		LOG("Invalid system address!");
		return;
	}

	const auto& it = m_ClientSessions.find(sysAddr);

	if (it == m_ClientSessions.end()) {
		LOG("Can not send to %s:%d, client not found!", sysAddr.ToString(false), sysAddr.port);
		return;
	}

	it->second->Send(bitStream);
}

void TcpTransportLayer::SendToManager(RakNet::BitStream& bitStream) {
	if (!m_ManagerConnectionActive) {
		throw std::runtime_error("Manager connection is not active!");
	}

	if (m_ManagerTransport) {
		m_ManagerTransport->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, m_ManagerSystemAddress, false);
	}
}

void TcpTransportLayer::Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID) {
	const auto& it = m_ClientSessions.find(sysAddr);

	if (it == m_ClientSessions.end()) {
		LOG("Client %s:%d not found in the session map!", sysAddr.ToString(false), sysAddr.port);
		return;
	}
	
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::SERVER, eServerMessageType::DISCONNECT_NOTIFY);
	bitStream.Write(disconNotifyID);

	it->second->Send(bitStream);

	m_ClientSessions.erase(it);
}

bool TcpTransportLayer::IsConnected(const SystemAddress& sysAddr) {
	return m_ClientSessions.find(sysAddr) != m_ClientSessions.end();
}

bool TcpTransportLayer::Startup() {
	m_VerifyCertificate = m_Config->GetValue("verify_certificate") == "1";

	if (m_UseTls) {
        m_SSLContext = std::make_unique<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        m_SSLContext->set_verify_mode(m_VerifyCertificate ? boost::asio::ssl::verify_peer : boost::asio::ssl::verify_none);
        
		if (m_Config->GetValue("cert_file").empty() || m_Config->GetValue("key_file").empty()) {
			LOG("Missing certificate or key file for TLS connection!");
			return false;
		}
		
		m_SSLContext->use_certificate_chain_file(m_Config->GetValue("cert_file"));
		m_SSLContext->use_private_key_file(m_Config->GetValue("key_file"), boost::asio::ssl::context::pem);
    }

	m_Acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(m_IOService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_Port));

    StartAccept();

    // Start the I/O service in a separate thread
    m_IOThread = std::thread([this]() {
        m_IOService.run();
    });

	if (m_ServerType != ServerType::Manager) {
		SetupForManagerConnection();
		if (!ConnectToManager()) {
			return false;
		}
	}

	return true;
}

void TcpTransportLayer::Shutdown() {
	// Disconnect all clients
	std::vector<SystemAddress> addresses;
	for (auto& [addr, session] : m_ClientSessions) {
		addresses.push_back(addr);
	}

	for (auto& addr : addresses) {
		Disconnect(addr, eServerDisconnectIdentifiers::SERVER_SHUTDOWN);
	}

	m_IOService.stop();

	try {
		if (m_IOThread.joinable()) {
			m_IOThread.join();
		}
	}
	catch (const std::exception& e) {
		LOG("Exception in io-thread: %s", e.what());
	}

	m_Acceptor->close();
}

void TcpTransportLayer::RemoveSession(const SystemAddress& sysAddr) {
	m_ClientSessions.erase(sysAddr);
}

void TcpTransportLayer::OnReceive(Packet* packet) {
	const auto& it = m_ClientSessions.find(packet->systemAddress);

	if (it == m_ClientSessions.end()) {
		LOG("Client %s:%d not found in the session map!", packet->systemAddress.ToString(false), packet->systemAddress.port);
		return;
	}

	const auto& session = it->second;

	const auto& type = packet->data[0];

	if (type == ID_USER_PACKET_ENUM) {
		std::unique_lock<std::mutex> lock(m_Mutex);

		m_ReceiveQueue.push(packet);

		return;
	}

	if (type == ID_DISCONNECTION_NOTIFICATION) {
		LOG("A client has disconnected");
		session->Close();
		RemoveSession(packet->systemAddress);
	} else if (type == ID_CONNECTION_LOST) {
		LOG("Lost our connection to a client");
		session->Close();
		RemoveSession(packet->systemAddress);
	} else if (type == ID_NEW_INCOMING_CONNECTION) {
		LOG("New incoming connection from %s:%d", packet->systemAddress.ToString(false), packet->systemAddress.port);
	} else if (type == ID_CONNECTION_REQUEST) {
		LOG("Connection request from %s:%d", packet->systemAddress.ToString(false), packet->systemAddress.port);
		
		const std::string password = std::string(reinterpret_cast<char*>(packet->data + 1), packet->length - 1);

		if (m_IsInternal) {
			if (password != NET_PASSWORD_INTERNAL) {
				LOG("Invalid internal password from %s:%d", packet->systemAddress.ToString(false), packet->systemAddress.port);
				Disconnect(packet->systemAddress, eServerDisconnectIdentifiers::WRONG_GAME_VERSION);
				return;
			}
		}
		else {
			if (password != NET_PASSWORD_EXTERNAL) {
				LOG("Invalid external password from %s:%d", packet->systemAddress.ToString(false), packet->systemAddress.port);
				Disconnect(packet->systemAddress, eServerDisconnectIdentifiers::WRONG_GAME_VERSION);
				return;
			}
		}

		RakNet::BitStream bitStream;
		bitStream.Write<uint8_t>(ID_CONNECTION_REQUEST_ACCEPTED);
		bitStream.Write<uint32_t>(packet->systemAddress.binaryAddress);
		bitStream.Write<uint16_t>(packet->systemAddress.port);
		bitStream.Write<uint16_t>(0);
		// Our ip and port
		bitStream.Write<uint32_t>(inet_addr(m_IP.c_str()));
		bitStream.Write<uint16_t>(m_Port);

		Send(bitStream, packet->systemAddress, false);

		session->ReadyForUserPackets();
	}
}

void TcpTransportLayer::SetupForManagerConnection() {

}

bool TcpTransportLayer::ConnectToManager() {
	if (m_ManagerTransport) {
		delete m_ManagerTransport;
		m_ManagerTransport = nullptr;
	}

	m_ManagerTransport = new TcpPeer(m_ManagerIP, m_ManagerPort, NET_PASSWORD_INTERNAL, m_UseTls, m_VerifyCertificate);

	if (!m_ManagerTransport->WaitForConnection()) {
		LOG("Failed to connect to Manager!");
		return false;
	}

	if (!m_ManagerTransport->IsConnected()) {
		LOG("Failed to connect to Manager!");
		return false;
	}

	m_ManagerConnectionActive = true;

	LOG("Connected to Manager %s:%d", m_ManagerIP.c_str(), m_ManagerPort);

	return true;
}

void TcpTransportLayer::StartAccept() {
	LOG("Listening for incoming connection on %s:%d", m_IP.c_str(), m_Port);

    auto session = std::make_shared<TcpSession>(m_IOService, m_SSLContext.get(), this);
    
    m_Acceptor->async_accept(session->GetSocket(), 
        [this, session](const boost::system::error_code& ec) { 
			HandleAccept(session, ec);
		}
	);
}

void TcpTransportLayer::HandleAccept(const std::shared_ptr<TcpSession>& session, const boost::system::error_code& ec) {
	if (!ec) {
		session->Start();

		SystemAddress sysAddr = session->GetSystemAddress();
		
		if (m_ClientSessions.find(sysAddr) != m_ClientSessions.end()) {
			LOG("Client already exists in the session map!");
			return;
		}

		m_ClientSessions[sysAddr] = session;

		LOG("[%i] Accepted connection from %s:%d", static_cast<int32_t>(m_ServerType), session->GetSocket().remote_endpoint().address().to_string().c_str(), session->GetSocket().remote_endpoint().port());
	} else {
		LOG("Accept error: %s", ec.message().c_str());
	}

	StartAccept();
}

int TcpTransportLayer::GetPing(const SystemAddress& sysAddr) const {
	const auto& it = m_ClientSessions.find(sysAddr);

	if (it == m_ClientSessions.end()) {
		LOG("Client %s:%d not found in the session map!", sysAddr.ToString(false), sysAddr.port);
		return 0;
	}

	return it->second->GetAveragePing();
}

int TcpTransportLayer::GetLatestPing(const SystemAddress& sysAddr) const {
	const auto& it = m_ClientSessions.find(sysAddr);

	if (it == m_ClientSessions.end()) {
		LOG("Client %s:%d not found in the session map!", sysAddr.ToString(false), sysAddr.port);
		return 0;
	}

	return it->second->GetLastPing();
}

TransportPeerInterface* TcpTransportLayer::CreateOutgoingTransport(
	uint32_t peerPort,
	const std::string& ip,
	uint32_t port,
	const std::string& password
) const {
	auto* peer = new TcpPeer(ip, port, password, m_UseTls, m_VerifyCertificate);

	if (!peer->WaitForConnection()) {
		LOG("Failed to connect to %s:%d", ip.c_str(), port);
		delete peer;
		throw std::runtime_error("Failed to connect to the server!");
	}

	return peer;
}

uint32_t TcpTransportLayer::ClaimSessionID() {
	return ++m_SessionID;
}