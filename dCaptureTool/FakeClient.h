#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "RakNetTypes.h"

class RakPeerInterface;

/**
 * A headless game client for the capture tool's replay: one RakNet connection to an auth or world server that sends
 * packets as given and keeps every packet it receives, with when it came. It speaks only RakNet and the LU packet
 * header; what to send comes from the bundle being replayed.
 */
class FakeClient {
public:
	struct Received {
		std::string bytes;
		int64_t timeUs{};
	};

	FakeClient();
	~FakeClient();
	FakeClient(const FakeClient&) = delete;
	FakeClient& operator=(const FakeClient&) = delete;

	bool Connect(const std::string& host, uint16_t port, std::chrono::milliseconds timeout);
	void Disconnect();
	bool IsConnected() const { return m_Connected; }

	void Send(const std::string& bytes);

	// Reads what arrived; false once the server closed the connection
	bool Pump();

	// Pumps until `until` returns true for a received packet (checked from `from` on) or the time is up. Returns
	// the index of that packet, or -1.
	int64_t WaitFor(const std::function<bool(const Received&)>& until, size_t from, std::chrono::milliseconds timeout);

	// Pumps for this long
	void Idle(std::chrono::milliseconds time);

	const std::vector<Received>& GetReceived() const { return m_Received; }
	uint16_t GetLocalPort() const { return m_LocalPort; }

private:
	RakPeerInterface* m_Peer{};
	SystemAddress m_Server{};
	bool m_Connected{};
	uint16_t m_LocalPort{};
	std::vector<Received> m_Received;
};
