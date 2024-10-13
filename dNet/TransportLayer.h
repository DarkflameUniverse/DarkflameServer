#pragma once
#include <string>
#include <csignal>

#include "RakNetTypes.h"
#include "TransportPeerInterface.h"

class Logger;
class dConfig;
enum class eServerDisconnectIdentifiers : uint32_t;

enum class ServerType : uint32_t {
	Manager,
	Auth,
	Chat,
	World
};

enum class ServiceId : uint32_t {
	General = 0,
	Auth = 1,
	Chat = 2,
	World = 4,
	Client = 5,
};

namespace Game {
	using signal_t = volatile std::sig_atomic_t;
}

/**
 * @brief Abstract base class for managing the network and transport layer.
 */
class TransportLayer {
public:
	/**
	 * @brief Receives a packet from the manager.
	 * @return Pointer to the received Packet.
	 */
	virtual Packet* ReceiveFromManager() = 0;

	/**
	 * @brief Deallocates a packet received from the manager.
	 * @param packet Pointer to the Packet to be deallocated.
	 */
	virtual void DeallocateManagerPacket(Packet* packet) = 0;

	/**
	 * @brief Sends a bit stream to the manager.
	 * @param bitStream Reference to the bitsteam to be sent.
	 */
	virtual void SendToManager(RakNet::BitStream& bitStream) = 0;

	/**
	 * @brief Receives a packet.
	 * @return Pointer to the received Packet.
	 */
	virtual Packet* Receive() = 0;

	/**
	 * @brief Deallocates a received packet.
	 * @param packet Pointer to the Packet to be deallocated.
	 */
	virtual void DeallocatePacket(Packet* packet) = 0;

	/**
	 * @brief Sends a bitstream to a specified system address.
	 * @param bitStream Reference to the RakNet::BitStream to be sent.
	 * @param sysAddr Reference to the SystemAddress to send the bit stream to.
	 * @param broadcast Whether to broadcast the bit stream.
	 */
	virtual void Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) = 0;

	/**
	 * @brief Disconnects from a specified system address.
	 * @param sysAddr Reference to the SystemAddress to disconnect from.
	 * @param disconNotifyID Identifier for the disconnection notification.
	 */
	virtual void Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID) = 0;

	/**
	 * @brief Checks if connected to a specified system address.
	 * @param sysAddr Reference to the SystemAddress to check connection status.
	 * @return Connection status, true if connected, false otherwise.
	 */
	virtual bool IsConnected(const SystemAddress& sysAddr) = 0;

	/**
	 * @brief Gets the IP address.
	 * @return Reference to the IP address string.
	 */
	virtual const std::string& GetIP() const = 0;

	/**
	 * @brief Gets the port number.
	 * @return The port number.
	 */
	virtual const int GetPort() const = 0;

	/**
	 * @brief Gets the maximum number of connections.
	 * @return The maximum number of connections.
	 */
	virtual const int GetMaxConnections() const = 0;

	/**
	 * @brief Checks if the connection is encrypted.
	 * @return Weather the connection is encrypted.
	 */
	virtual const bool GetIsEncrypted() const = 0;

	/**
	 * @brief Checks if the connection is internal.
	 * @return Weather the connection is internal.
	 */
	virtual const bool GetIsInternal() const = 0;

	/**
	 * @brief Checks if the connection is okay.
	 * @return Weather the connection is okay.
	 */
	virtual const bool GetIsOkay() const = 0;

	/**
	 * @brief Gets the logger instance.
	 * @return Pointer to the Logger instance.
	 */
	virtual Logger* GetLogger() const = 0;

	/**
	 * @brief Checks if connected to the manager.
	 * @return Weather we are connected to the manager.
	 */
	virtual const bool GetIsConnectedToManager() const = 0;

	/**
	 * @brief Gets the zone ID.
	 * @return The zone ID.
	 */
	virtual const uint32_t GetZoneID() const = 0;

	/**
	 * @brief Gets the instance ID.
	 * @return The instance ID.
	 */
	virtual const int GetInstanceID() const = 0;

	/**
	 * @brief Gets the ping to a specified system address.
	 * @param sysAddr Reference to the SystemAddress to get the ping for.
	 * @return The ping.
	 */
	virtual int GetPing(const SystemAddress& sysAddr) const = 0;

	/**
	 * @brief Gets the latest ping to a specified system address.
	 * @param sysAddr Reference to the SystemAddress to get the latest ping for.
	 * @return The latest ping.
	 */
	virtual int GetLatestPing(const SystemAddress& sysAddr) const = 0;

	/**
	 * @brief Gets the server type.
	 * @return The server type.
	 */
	virtual const ServerType GetServerType() const = 0;

	/**
	 * @brief Starts up the transport layer.
	 * 
	 * @return True if successful, false otherwise.
	 */
	virtual bool Startup() = 0;

	/**
	 * @brief Shuts down the transport layer.
	 */
	virtual void Shutdown() = 0;

	/**
	 * @brief Create an outgoing transport connected to a server.
	 * 
	 * @param ip The IP address of the server.
	 * @param port The port of the server.
	 * @param password The password to connect to the server.
	 */
	virtual TransportPeerInterface* CreateOutgoingTransport(
		uint32_t peerPort,
		const std::string& ip,
		uint32_t port,
		const std::string& password
	) const = 0;
};
