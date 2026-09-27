#include "FakeClient.h"

#include <cstring>
#include <thread>

#include "dNetCommon.h"
#include "MessageIdentifiers.h"
#include "RakNetworkFactory.h"
#include "RakPeerInterface.h"
#include "PacketPriority.h"

namespace {
	int64_t NowUs() {
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}

FakeClient::FakeClient() = default;

FakeClient::~FakeClient() { Disconnect(); }

bool FakeClient::Connect(const std::string& host, uint16_t port, std::chrono::milliseconds timeout) {
	Disconnect();
	m_Peer = RakNetworkFactory::GetRakPeerInterface();
	SocketDescriptor socket(0, nullptr);
	if (!m_Peer->Startup(1, 10, &socket, 1)) return false;
	m_LocalPort = m_Peer->GetInternalID().port;
	if (!m_Peer->Connect(host.c_str(), port, NET_PASSWORD_EXTERNAL, static_cast<int>(strnlen(NET_PASSWORD_EXTERNAL, sizeof(NET_PASSWORD_EXTERNAL))))) return false;
	const auto until = std::chrono::steady_clock::now() + timeout;
	while (std::chrono::steady_clock::now() < until) {
		for (Packet* packet = m_Peer->Receive(); packet; packet = m_Peer->Receive()) {
			const auto id = packet->length ? packet->data[0] : 0;
			if (id == ID_CONNECTION_REQUEST_ACCEPTED) {
				m_Server = packet->systemAddress;
				m_Connected = true;
			}
			m_Peer->DeallocatePacket(packet);
			if (id == ID_CONNECTION_ATTEMPT_FAILED || id == ID_NO_FREE_INCOMING_CONNECTIONS || id == ID_INVALID_PASSWORD) return false;
			if (m_Connected) return true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return false;
}

void FakeClient::Disconnect() {
	if (!m_Peer) return;
	if (m_Connected) m_Peer->CloseConnection(m_Server, true);
	m_Peer->Shutdown(100);
	RakNetworkFactory::DestroyRakPeerInterface(m_Peer);
	m_Peer = nullptr;
	m_Connected = false;
}

void FakeClient::Send(const std::string& bytes) {
	if (!m_Connected || bytes.empty()) return;
	m_Peer->Send(bytes.data(), static_cast<int>(bytes.size()), SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, m_Server, false);
}

bool FakeClient::Pump() {
	if (!m_Peer) return false;
	for (Packet* packet = m_Peer->Receive(); packet; packet = m_Peer->Receive()) {
		if (packet->length) {
			const auto id = packet->data[0];
			if (id == ID_DISCONNECTION_NOTIFICATION || id == ID_CONNECTION_LOST) m_Connected = false;
			// RakNet's own connection messages aren't the server's answers
			if (id == ID_USER_PACKET_ENUM || (id >= ID_REPLICA_MANAGER_CONSTRUCTION && id <= ID_REPLICA_MANAGER_DOWNLOAD_COMPLETE)) {
				m_Received.push_back({ std::string(reinterpret_cast<const char*>(packet->data), packet->length), NowUs() });
			}
		}
		m_Peer->DeallocatePacket(packet);
	}
	return m_Connected;
}

int64_t FakeClient::WaitFor(const std::function<bool(const Received&)>& until, size_t from, std::chrono::milliseconds timeout) {
	const auto end = std::chrono::steady_clock::now() + timeout;
	size_t checked = from;
	while (true) {
		const bool open = Pump();
		for (; checked < m_Received.size(); checked++) {
			if (until(m_Received[checked])) return static_cast<int64_t>(checked);
		}
		if (!open || std::chrono::steady_clock::now() >= end) return -1;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

void FakeClient::Idle(std::chrono::milliseconds time) {
	const auto end = std::chrono::steady_clock::now() + time;
	while (std::chrono::steady_clock::now() < end) {
		if (!Pump()) return;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}
