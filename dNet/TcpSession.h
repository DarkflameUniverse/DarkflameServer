#pragma once

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <memory>
#include <chrono>
#include "RakNetTypes.h"
#include "eServerDisconnectIdentifiers.h"
#include "BitStream.h"

class TcpTransportLayer;

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
	TcpSession(boost::asio::io_service& io_service, boost::asio::ssl::context* sslContext, TcpTransportLayer* server);
	void Start();
	void Send(const std::vector<uint8_t>& data);
	void Send(const RakNet::BitStream& data);

	boost::asio::basic_socket<boost::asio::ip::tcp, boost::asio::any_io_executor>& GetSocket();

	bool IsConnected() const { return m_IsConnected; }

	const SystemAddress& GetSystemAddress() const { return m_SystemAddress; }

	bool IsReadyForUserPackets() const { return m_ReadyForUserPackets; }

	void ReadyForUserPackets();

	float GetAveragePing() const;

	float GetLastPing() const;

	void Close();

private:
	void StartAsyncRead();
	
	void OnReadHeader(const boost::system::error_code& ec, std::size_t bytesTransferred);
	
	void OnReadBody(const boost::system::error_code& ec, std::size_t bytesTransferred);
	
	void OnWrite(const boost::system::error_code& ec, std::size_t bytesTransferred);

	void HandlePing(Packet* packet);

	void HandlePong(Packet* packet);

	void DoPing();

	std::unique_ptr<boost::asio::ip::tcp::socket> m_Socket;
	std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_SSLStream;
	bool m_UseTls;
	TcpTransportLayer* m_Server;
	SystemAddress m_SystemAddress;
	bool m_IsConnected;
	bool m_ReadyForUserPackets;
	std::queue<Packet*> m_UserPacketQueue;

	std::vector<uint8_t> m_HeaderBuffer;
	std::vector<uint8_t> m_BodyBuffer;

	// Ping handling
	int32_t m_CumulativePing;
	int32_t m_PingCount;
	int32_t m_LastPing;
	int32_t m_PingTimer;
	boost::asio::steady_timer m_PingTimerObject;

	// Last ping time
	std::chrono::time_point<std::chrono::high_resolution_clock> m_ChronoLastPingTime;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_ChronoLastPongTime;
	// Accumulated ping time
	std::chrono::milliseconds m_AccumulatedPingTime;
};