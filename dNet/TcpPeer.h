#pragma once

#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <memory>
#include <condition_variable>
#include "TransportPeerInterface.h"

class TcpPeer : public TransportPeerInterface {
public:
    TcpPeer(const std::string& ip, int port, const std::string& password, bool useTls, bool verifyCertificate);
    ~TcpPeer();

    void Send(
        const RakNet::BitStream* bitStream,
        PacketPriority priority,
        PacketReliability reliability,
        char orderingChannel,
        SystemAddress systemAddress,
        bool broadcast
    ) override;

    void Disconnect() override;
    void Reconnect() override;

    Packet* Receive() override;
    void DeallocatePacket(Packet* packet) override;

	bool IsConnected();
	bool WaitForConnection();

private:
    void StartAsyncRead();
    void OnReadHeader(const boost::system::error_code& ec, std::size_t bytesTransferred);
    void OnReadBody(const boost::system::error_code& ec, std::size_t bytesTransferred);
    void HandleConnect(const boost::system::error_code& ec);

	void SendConnectionRequest();

	void OnReceive(Packet* packet);
    
    boost::asio::io_service m_IOService;
    boost::asio::ip::tcp::resolver m_Resolver;
    std::unique_ptr<boost::asio::ip::tcp::socket> m_Socket;
    std::unique_ptr<boost::asio::ssl::context> m_SSLContext;
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_SSLStream;


    std::string m_IP;
    int m_Port;
    std::string m_Password;
    bool m_UseTls;
    bool m_VerifyCertificate;
    SystemAddress m_PeerSystemAddress;
    bool m_IsConnected;

    std::mutex m_ConnectMutex;
    std::mutex m_Mutex;
	std::condition_variable m_Condition;

    std::queue<Packet*> m_ReceiveBuffer;

    std::vector<uint8_t> m_HeaderBuffer;
    std::vector<uint8_t> m_BodyBuffer;

    std::thread m_IOThread;
    boost::asio::io_service::work m_Work;
};
