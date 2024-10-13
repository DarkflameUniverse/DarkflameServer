#include "TcpSession.h"

#include "TcpTransportLayer.h"
#include "Logger.h"
#include "Game.h"
#include "MessageIdentifiers.h"

#include <boost/endian/conversion.hpp>

TcpSession::TcpSession(boost::asio::io_service& io_service, boost::asio::ssl::context* sslContext, TcpTransportLayer* server):
	m_PingTimerObject(io_service) {
	m_Server = server;
	
	m_UseTls = server->GetIsEncrypted();
	
	if (m_UseTls) {
		m_SSLStream = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(io_service, *sslContext);
	}
	else {
		m_Socket = std::make_unique<boost::asio::ip::tcp::socket>(io_service);
	}

	m_IsConnected = false;
	m_ReadyForUserPackets = false;
	m_PingCount = 0;
	m_CumulativePing = 0;
	m_LastPing = 0;
	m_PingTimer = 0;
}

void TcpSession::Start() {
	if (m_UseTls) {
		m_SSLStream->async_handshake(boost::asio::ssl::stream_base::server,
			[this](const boost::system::error_code& ec) {
				if (!ec) {
					m_SystemAddress.port = m_SSLStream->lowest_layer().remote_endpoint().port();
					if (m_SSLStream->lowest_layer().remote_endpoint().address().is_v4()) {
						m_SystemAddress.binaryAddress = m_SSLStream->lowest_layer().remote_endpoint().address().to_v4().to_uint();
					} else {
						m_SystemAddress.binaryAddress = 0;
					}

					m_IsConnected = true;

					StartAsyncRead();
				}
			});
	} else {
		m_SystemAddress.port = m_Socket->remote_endpoint().port();
		if (m_Socket->remote_endpoint().address().is_v4()) {
			uint32_t address = m_Socket->remote_endpoint().address().to_v4().to_uint();
			m_SystemAddress.binaryAddress = boost::endian::endian_reverse(address);
		} else {
			m_SystemAddress.binaryAddress = 0;
		}

		m_IsConnected = true;

		StartAsyncRead();
	}

	// Start the ping timer in 5 seconds using boost
	m_PingTimerObject.expires_after(std::chrono::seconds(5));
	m_PingTimerObject.async_wait([this](const boost::system::error_code& ec) {
		if (!ec) {
			DoPing();
		}
	});
}

void TcpSession::Send(const std::vector<uint8_t>& data) {
	auto self(shared_from_this());
	auto writeCallback = [this, self](const boost::system::error_code& ec, std::size_t /*bytesTransferred*/) {
		OnWrite(ec, 0);  // Handle the write result
	};

	uint32_t size = data.size();

	if (m_UseTls) {
		// Write the size of the message
		boost::asio::async_write(*m_SSLStream, boost::asio::buffer(&size, sizeof(size)), writeCallback);
		boost::asio::async_write(*m_SSLStream, boost::asio::buffer(data), writeCallback);
	} else {
		// Write the size of the message
		boost::asio::async_write(*m_Socket, boost::asio::buffer(&size, sizeof(size)), writeCallback);
		boost::asio::async_write(*m_Socket, boost::asio::buffer(data), writeCallback);
	}
}

void TcpSession::Send(const RakNet::BitStream& data) {
	auto self(shared_from_this());
	auto writeCallback = [this, self](const boost::system::error_code& ec, std::size_t /*bytesTransferred*/) {
		OnWrite(ec, 0);  // Handle the write result
	};

	uint32_t size = data.GetNumberOfBytesUsed();

	if (m_UseTls) {
		// Write the size of the message
		boost::asio::async_write(*m_SSLStream, boost::asio::buffer(&size, sizeof(size)), writeCallback);
		boost::asio::async_write(*m_SSLStream, boost::asio::buffer(data.GetData(), size), writeCallback);
	} else {
		// Write the size of the message
		boost::asio::async_write(*m_Socket, boost::asio::buffer(&size, sizeof(size)), writeCallback);
		boost::asio::async_write(*m_Socket, boost::asio::buffer(data.GetData(), size), writeCallback);
	}
}

boost::asio::basic_socket<boost::asio::ip::tcp, boost::asio::any_io_executor>& TcpSession::GetSocket() {
	if (m_UseTls) {
		return m_SSLStream->lowest_layer();
	} else {
		return *m_Socket;
	}
}

void TcpSession::StartAsyncRead() {
	if (!m_IsConnected) {
		return;
	}

	m_HeaderBuffer.resize(sizeof(uint32_t));

	auto readHeaderCallback = [this](const boost::system::error_code& ec, std::size_t bytesTransferred) {
		OnReadHeader(ec, bytesTransferred);
	};

	try {
		if (m_UseTls) {
			boost::asio::async_read(*m_SSLStream, boost::asio::buffer(m_HeaderBuffer), readHeaderCallback);
		} else {
			boost::asio::async_read(*m_Socket, boost::asio::buffer(m_HeaderBuffer), readHeaderCallback);
		}
	} catch (const std::exception& e) {
		LOG("Exception: %s", e.what());
		Close();
	}
}

void TcpSession::OnReadHeader(const boost::system::error_code& ec, std::size_t bytesTransferred) {
	if (!ec) {
		uint32_t size = 0;
		std::memcpy(&size, m_HeaderBuffer.data(), sizeof(size));

		m_BodyBuffer.resize(size);

		auto readBodyCallback = [this](const boost::system::error_code& ec, std::size_t bytesTransferred) {
			OnReadBody(ec, bytesTransferred);
		};

		if (m_UseTls) {
			boost::asio::async_read(*m_SSLStream, boost::asio::buffer(m_BodyBuffer), readBodyCallback);
		} else {
			boost::asio::async_read(*m_Socket, boost::asio::buffer(m_BodyBuffer), readBodyCallback);
		}
	} else {
		LOG("Read header error: %s", ec.message().c_str());
		Close();
	}
}

void TcpSession::OnReadBody(const boost::system::error_code& ec, std::size_t bytesTransferred) {
	if (!ec) {
		auto* packet = new Packet();
		packet->data = new uint8_t[m_BodyBuffer.size()];
		std::memcpy(packet->data, m_BodyBuffer.data(), m_BodyBuffer.size());
		packet->length = m_BodyBuffer.size();
		packet->bitSize = packet->length * 8;
		packet->systemAddress = m_SystemAddress;

		if (packet->data[0] == ID_USER_PACKET_ENUM && !m_ReadyForUserPackets) {
			LOG("Received early user packet from %s:%d", m_SystemAddress.ToString(false), m_SystemAddress.port);
			m_UserPacketQueue.push(packet);
		}
		else if (packet->data[0] == ID_INTERNAL_PING) {
			HandlePing(packet);
		}
		else if (packet->data[0] == ID_CONNECTED_PONG) {
			HandlePong(packet);
		}
		else {
			m_Server->OnReceive(packet);
		}

		StartAsyncRead();
	} else {
		LOG("Read body error: %s", ec.message().c_str());
		Close();
	}
}

void TcpSession::OnWrite(const boost::system::error_code& ec, std::size_t bytesTransferred) {
	if (ec) {
		LOG("Write error: %s", ec.message().c_str());
		Close();
	}
}

void TcpSession::HandlePing(Packet* packet) {
	RakNet::BitStream bitStream;
	bitStream.Write<uint8_t>(ID_CONNECTED_PONG);
	bitStream.Write<uint8_t>(packet->data[1]);
	bitStream.Write<uint8_t>(packet->data[2]);
	bitStream.Write<uint8_t>(packet->data[3]);
	bitStream.Write<uint8_t>(packet->data[4]);
	bitStream.Write<uint32_t>(0);

	Send(bitStream);
}

void TcpSession::HandlePong(Packet* packet) {
	auto current = m_PingTimer;

	RakNet::BitStream bitStream(packet->data, packet->length, false);
	bitStream.IgnoreBytes(1);

	uint32_t old = 0;
	bitStream.Read(old);

	m_LastPing = current - old;
	m_CumulativePing += m_LastPing;
	m_PingCount++;

	const auto now = std::chrono::high_resolution_clock::now();
	const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_ChronoLastPingTime);
	m_AccumulatedPingTime += duration;

	m_ChronoLastPongTime = now;
}

void TcpSession::DoPing() {
	if (!m_IsConnected) {
		return;
	}

	RakNet::BitStream bitStream;
	bitStream.Write<uint8_t>(ID_INTERNAL_PING);
	bitStream.Write<uint32_t>(m_PingTimer);

	Send(bitStream);

	m_ChronoLastPingTime = std::chrono::high_resolution_clock::now();

	// Start the ping timer in 5 seconds using boost
	m_PingTimerObject.expires_after(std::chrono::seconds(5));
	m_PingTimerObject.async_wait([this](const boost::system::error_code& ec) {
		if (!ec) {
			DoPing();
		}
	});
}

void TcpSession::ReadyForUserPackets() {
	m_ReadyForUserPackets = true;

	while (!m_UserPacketQueue.empty()) {
		auto* packet = m_UserPacketQueue.front();
		m_UserPacketQueue.pop();

		m_Server->OnReceive(packet);
	}
}

float TcpSession::GetAveragePing() const {
	if (m_PingCount == 0) {
		return 0.0f;
	}

	return static_cast<float>(m_AccumulatedPingTime.count()) / m_PingCount;
}

float TcpSession::GetLastPing() const {
	return static_cast<float>(m_LastPing);
}

void TcpSession::Close() {
	if (!m_IsConnected) {
		return;
	}

	LOG("Closing connection to %s:%d", m_SystemAddress.ToString(false), m_SystemAddress.port);

	if (m_UseTls) {
		m_SSLStream->async_shutdown([this](const boost::system::error_code& ec) {
			if (ec) {
				LOG("SSL shutdown error: %s", ec.message().c_str());
			}

			m_SSLStream->lowest_layer().close();

			m_IsConnected = false;

			m_SSLStream.reset();
		});
	} else {
		m_Socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
		m_Socket->close();

		m_IsConnected = false;

		m_Socket.reset();
	}

	m_PingTimerObject.cancel();

	m_Server->RemoveSession(m_SystemAddress);
}