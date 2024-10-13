#include "TcpPeer.h"

#include <RakNetTypes.h>
#include <BitStream.h>
#include <random>
#include <MessageIdentifiers.h>
#include <magic_enum.hpp>

#include "Logger.h"
#include "Game.h"


TcpPeer::TcpPeer(const std::string& ip, int port, const std::string& password, bool useTls, bool verifyCertificate)
    : m_IP(ip), m_Port(port), m_Password(password), m_UseTls(useTls), m_VerifyCertificate(verifyCertificate),
      m_Resolver(m_IOService), m_Work(m_IOService), m_IsConnected(false)
{
	if (m_IP == "localhost") {
		m_IP = "127.0.0.1";
	}

    // Start the I/O thread for asynchronous operations
    m_IOThread = std::thread([this]() {
        m_IOService.run();
    });

	Reconnect();
}

TcpPeer::~TcpPeer() {
    Disconnect();

    // Ensure the I/O thread joins
    if (m_IOThread.joinable()) {
        m_IOThread.join();
    }
}

void TcpPeer::Reconnect() {
    Disconnect();

	std::scoped_lock lock(m_ConnectMutex);
    
    boost::asio::ip::tcp::resolver::query query(m_IP, std::to_string(m_Port));
    auto endpointIterator = m_Resolver.resolve(query);

	if (!endpointIterator->endpoint().address().is_v4()) {
		LOG("Failed to resolve IP address!");
		return;
	}

	// Print out the resolved IP and port
	LOG("Resolved IP: %s, Port: %d", endpointIterator->endpoint().address().to_v4().to_string().c_str(), endpointIterator->endpoint().port());

	// Just use a random 32-bit integer for the system address
	m_PeerSystemAddress.binaryAddress = endpointIterator->endpoint().address().to_v4().to_uint();
	m_PeerSystemAddress.port = endpointIterator->endpoint().port();

    if (m_UseTls) {
        m_SSLContext = std::make_unique<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        m_SSLContext->set_verify_mode(m_VerifyCertificate ? boost::asio::ssl::verify_peer : boost::asio::ssl::verify_none);
        m_SSLStream = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(m_IOService, *m_SSLContext); 
        m_SSLStream->set_verify_callback(boost::asio::ssl::rfc2818_verification(m_IP));

        boost::asio::async_connect(m_SSLStream->lowest_layer(), endpointIterator,
            [this, &lock](const boost::system::error_code& ec, auto) { HandleConnect(ec); });
    } else {
        m_Socket = std::make_unique<boost::asio::ip::tcp::socket>(m_IOService);
        boost::asio::async_connect(*m_Socket, endpointIterator,
            [this, &lock](const boost::system::error_code& ec, auto) { HandleConnect(ec); });
    }
}

void TcpPeer::HandleConnect(const boost::system::error_code& ec) {
    if (!ec) {
        if (m_UseTls) {
            m_SSLStream->async_handshake(boost::asio::ssl::stream_base::client,
                [this](const boost::system::error_code& handshakeError) {
                    if (!handshakeError) {
						SendConnectionRequest();
                        StartAsyncRead();
                    }
					else {
						LOG("Handshake error: %s", handshakeError.message().c_str());
					}
                });
        } else {
			SendConnectionRequest();
            StartAsyncRead();
        }
    }
	else {
		m_IsConnected = false;

		m_Condition.notify_all();

		LOG("Connect error: %s", ec.message().c_str());
	}
}

void TcpPeer::SendConnectionRequest() {
	RakNet::BitStream bitStream;
	bitStream.Write<uint8_t>(ID_CONNECTION_REQUEST);
	for (const auto& c : m_Password) {
		bitStream.Write<uint8_t>(c);
	}

	Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_PeerSystemAddress, false);
}

void TcpPeer::OnReceive(Packet* packet) {
	if (packet->length == 0) {
		delete[] packet->data;
		delete packet;
		return;
	}
	
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
		Disconnect();
	}
	else if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) {
        m_IsConnected = true;
		m_Condition.notify_all();
	}

	{
		std::scoped_lock lock(m_Mutex);

		m_ReceiveBuffer.push(packet);
	}
}

bool TcpPeer::IsConnected() {
	std::scoped_lock lock(m_ConnectMutex);

	return m_IsConnected;
}

bool TcpPeer::WaitForConnection() {
	std::unique_lock<std::mutex> lock(m_ConnectMutex);

	if (m_IsConnected) {
		LOG("Already connected to the server!");
		return true;
	}

	if (!m_Condition.wait_for(lock, std::chrono::seconds(5), [this]() { return m_IsConnected; })) {
		LOG("Failed to connect to the server!");
		return false;
	}

	return m_IsConnected;
}

void TcpPeer::StartAsyncRead() {
    m_HeaderBuffer.resize(sizeof(uint32_t));

    auto readHeaderCallback = [this](const boost::system::error_code& ec, std::size_t bytesTransferred) {
        OnReadHeader(ec, bytesTransferred);
    };

    if (m_UseTls) {
        boost::asio::async_read(*m_SSLStream, boost::asio::buffer(m_HeaderBuffer), readHeaderCallback);
    } else {
        boost::asio::async_read(*m_Socket, boost::asio::buffer(m_HeaderBuffer), readHeaderCallback);
    }
}

void TcpPeer::OnReadHeader(const boost::system::error_code& ec, std::size_t bytesTransferred) {
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

        m_IsConnected = false;
    }
}

void TcpPeer::OnReadBody(const boost::system::error_code& ec, std::size_t bytesTransferred) {
    if (!ec) {
        auto* packet = new Packet();
        packet->data = new uint8_t[m_BodyBuffer.size()];
        std::memcpy(packet->data, m_BodyBuffer.data(), m_BodyBuffer.size());
        packet->length = m_BodyBuffer.size();
        packet->bitSize = packet->length * 8;
        packet->systemAddress = m_PeerSystemAddress;

		OnReceive(packet);

        // Start reading the next packet
        StartAsyncRead();
    } else {
		LOG("Read body error: %s", ec.message().c_str());

        m_IsConnected = false;
    }
}

void TcpPeer::Send(
    const RakNet::BitStream* bitStream,
    PacketPriority priority,
    PacketReliability reliability,
    char orderingChannel,
    SystemAddress systemAddress,
    bool broadcast
) {
    auto* data = bitStream->GetData();
    uint32_t size = bitStream->GetNumberOfBytesUsed();
    std::vector<uint8_t> sendBuffer(sizeof(uint32_t) + size);

    std::memcpy(sendBuffer.data(), &size, sizeof(size));
    std::memcpy(sendBuffer.data() + sizeof(uint32_t), data, size);

    auto sendCallback = [](const boost::system::error_code& ec, std::size_t /*bytesTransferred*/) {
        if (ec) {
			LOG("Send error: %s", ec.message().c_str());
        }
    };

    if (m_UseTls) {
        boost::asio::async_write(*m_SSLStream, boost::asio::buffer(sendBuffer), sendCallback);
    } else {
        boost::asio::async_write(*m_Socket, boost::asio::buffer(sendBuffer), sendCallback);
    }
}

Packet* TcpPeer::Receive() {
    std::unique_lock<std::mutex> lock(m_Mutex);

	if (m_ReceiveBuffer.empty()) {
		return nullptr;
	}

    auto* packet = m_ReceiveBuffer.front();
    m_ReceiveBuffer.pop();

    return packet;
}

void TcpPeer::Disconnect() {
    std::scoped_lock lock(m_Mutex);

    if (m_IsConnected) {
        if (m_UseTls) {
            m_SSLStream->lowest_layer().close();
        } else {
            m_Socket->close();
        }

        m_IsConnected = false;
    }
}

void TcpPeer::DeallocatePacket(Packet* packet) {
    delete[] packet->data;
    delete packet;
}