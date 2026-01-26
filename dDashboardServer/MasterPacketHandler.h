#pragma once

#include <functional>
#include <map>
#include <memory>

#include "dCommonVars.h"
#include "MessageType/Master.h"
#include "BitStream.h"

struct Packet;

namespace MasterPacketHandler {
	// Base class for all master packet handlers
	class MasterPacket {
	public:
		virtual ~MasterPacket() = default;
		virtual bool Deserialize(RakNet::BitStream& bitStream) = 0;
		virtual void Handle() = 0;
	};

	// SERVER_INFO packet handler
	class ServerInfo : public MasterPacket {
	public:
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;

	private:
		uint32_t port{0};
		uint32_t zoneID{0};
		uint32_t instanceID{0};
		ServiceType serverType{};
		std::string ip{};
	};

	// PLAYER_ADDED packet handler
	class PlayerAdded : public MasterPacket {
	public:
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;

	private:
		LWOMAPID zoneID{};
		LWOINSTANCEID instanceID{};
	};

	// PLAYER_REMOVED packet handler
	class PlayerRemoved : public MasterPacket {
	public:
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;

	private:
		LWOMAPID zoneID{};
		LWOINSTANCEID instanceID{};
	};

	// SHUTDOWN_RESPONSE packet handler
	class ShutdownResponse : public MasterPacket {
	public:
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;

	private:
		uint32_t zoneID{};
		uint32_t instanceID{};
		ServiceType serverType{};
	};

	// SHUTDOWN packet handler
	class Shutdown : public MasterPacket {
	public:
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	// Main handler function
	void HandleMasterPacket(Packet* packet);
}
