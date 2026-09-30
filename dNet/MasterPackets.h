#ifndef MASTERPACKETS_H
#define MASTERPACKETS_H

#include <cstdint>
#include <string>
#include <vector>
#include "RakNetTypes.h"
#include "dCommonVars.h"
#include "BitStreamUtils.h"
#include "MessageType/Master.h"
#include "ServiceType.h"
#include "Stamps.h"
#include "master/DashboardMessages.h"
#include "master/DataChanged.h"
#include "master/InstanceMigration.h"
#include "master/LiveUpdate.h"
#include "master/MessageCapture.h"
#include "master/PlayerAction.h"
#include "master/UgcModelsMade.h"

class dServer;

/**
 * Packets between master and the other servers (MessageType::Master). None of them reach a client.
 *
 * The dashboard's messages and instance migration are MASTER packets too; they live in per-topic headers under
 * dNet/master/ (PlayerAction.h, DataChanged.h, DashboardMessages.h, MessageCapture.h, InstanceMigration.h,
 * LiveUpdate.h, UgcModelsMade.h), which
 * this header includes, so including MasterPackets.h gives every MASTER struct.
 */
namespace MasterPackets {
	// Any server -> master, over server's connection to master (Game::server's when null)
	void SendToMaster(const LUBitStream& msg, dServer* server = nullptr);

	// Master -> one server. Never broadcasts: nothing is sent to UNASSIGNED_SYSTEM_ADDRESS (a server that hasn't
	// connected yet), unlike LUBitStream::Send.
	void SendTo(const SystemAddress& sysAddr, const LUBitStream& msg);

	// World -> master: a player wants to go to zoneID (clone cloneID)
	struct RequestZoneTransfer : public LUBitStream {
		uint64_t requestID{};
		uint8_t mythranShift{};
		uint32_t zoneID{};
		uint32_t cloneID{};
		// The login stamps travelling with the request (see Stamps.h); empty unless auth asks for a world during a
		// login. Read leniently: a request without them reads as empty.
		Stamps stamps{};

		RequestZoneTransfer() : LUBitStream(ServiceType::MASTER, MessageType::Master::REQUEST_ZONE_TRANSFER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Master -> world: where to send the player of requestID
	struct RequestZoneTransferResponse : public LUBitStream {
		uint64_t requestID{};
		uint8_t mythranShift{};
		uint32_t zoneID{};
		uint32_t zoneInstance{};
		uint32_t zoneClone{};
		uint16_t serverPort{};
		LUString serverIP{ 255u };
		// The request's login stamps with master's own added (see Stamps.h). Read leniently like the request's.
		Stamps stamps{};

		RequestZoneTransferResponse() : LUBitStream(ServiceType::MASTER, MessageType::Master::REQUEST_ZONE_TRANSFER_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Any server -> master, once connected; master -> dashboard for every other server (ip "offline" when one is gone)
	struct ServerInfo : public LUBitStream {
		uint32_t port{};
		uint32_t zoneID{};
		uint32_t instanceID{};
		ServiceType serverType{};
		LUString ip{};

		ServerInfo() : LUBitStream(ServiceType::MASTER, MessageType::Master::SERVER_INFO) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> master: the session key of the account logging in
	struct RequestSessionKey : public LUBitStream {
		LUWString username{};

		RequestSessionKey() : LUBitStream(ServiceType::MASTER, MessageType::Master::REQUEST_SESSION_KEY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Auth -> master: an account logged in with this session key
	struct SetSessionKey : public LUBitStream {
		uint32_t sessionKey{};
		LUString username{};

		SetSessionKey() : LUBitStream(ServiceType::MASTER, MessageType::Master::SET_SESSION_KEY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Master -> world: answer to RequestSessionKey
	struct SessionKeyResponse : public LUBitStream {
		uint32_t sessionKey{};
		LUWString username{};

		SessionKeyResponse() : LUBitStream(ServiceType::MASTER, MessageType::Master::SESSION_KEY_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Master -> every server: the account logged in again with a new session key
	struct NewSessionAlert : public LUBitStream {
		uint32_t sessionKey{};
		LUString username{};

		NewSessionAlert() : LUBitStream(ServiceType::MASTER, MessageType::Master::NEW_SESSION_ALERT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> master -> dashboard: a player came into (PLAYER_ADDED) or left (PLAYER_REMOVED) a world
	struct PlayerCountChange : public LUBitStream {
		LWOMAPID zoneID{};
		LWOINSTANCEID instanceID{};

		PlayerCountChange(MessageType::Master id) : LUBitStream(ServiceType::MASTER, id) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct PlayerAdded : public PlayerCountChange {
		PlayerAdded() : PlayerCountChange(MessageType::Master::PLAYER_ADDED) {}
	};

	struct PlayerRemoved : public PlayerCountChange {
		PlayerRemoved() : PlayerCountChange(MessageType::Master::PLAYER_REMOVED) {}
	};

	// World -> master: start a private instance of zoneID that needs `password` to join
	struct CreatePrivateZone : public LUBitStream {
		// Longer passwords are cut to this when read
		static constexpr uint32_t MAX_PASSWORD_LENGTH = 50;

		uint32_t zoneID{};
		uint32_t cloneID{};
		std::string password; // u32 length, then 1 byte per character

		CreatePrivateZone() : LUBitStream(ServiceType::MASTER, MessageType::Master::CREATE_PRIVATE_ZONE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> master: send the player of requestID to the private instance with this password
	struct RequestPrivateZone : public LUBitStream {
		// Longer passwords are cut to this when read
		static constexpr uint32_t MAX_PASSWORD_LENGTH = 50;

		uint64_t requestID{};
		uint8_t mythranShift{};
		std::string password; // u32 length, then 1 byte per character

		RequestPrivateZone() : LUBitStream(ServiceType::MASTER, MessageType::Master::REQUEST_PRIVATE_ZONE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> master: the world finished loading and takes players
	struct WorldReady : public LUBitStream {
		LWOMAPID zoneID{};
		LWOINSTANCEID instanceID{};

		WorldReady() : LUBitStream(ServiceType::MASTER, MessageType::Master::WORLD_READY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Master -> dashboard (WORLD_READY): a world is ready, and where it is
	struct WorldReadyInfo : public LUBitStream {
		LWOMAPID zoneID{};
		LWOINSTANCEID instanceID{};
		LWOCLONEID cloneID{};
		LUString ip{};
		uint32_t port{};
		uint8_t isPrivate{};

		WorldReadyInfo() : LUBitStream(ServiceType::MASTER, MessageType::Master::WORLD_READY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> master: start an instance of zoneID now, players are about to ask for it. cloneID: the clone they will ask
	// for (a property), written only when not 0 so a plain prep stays as it always was
	struct PrepZone : public LUBitStream {
		int32_t zoneID{};
		uint32_t cloneID{};

		PrepZone() : LUBitStream(ServiceType::MASTER, MessageType::Master::PREP_ZONE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Master -> any server: shut down
	struct Shutdown : public LUBitStream {
		Shutdown() : LUBitStream(ServiceType::MASTER, MessageType::Master::SHUTDOWN) {}
	};

	// World -> master: shutting down
	struct ShutdownResponse : public LUBitStream {
		ShutdownResponse() : LUBitStream(ServiceType::MASTER, MessageType::Master::SHUTDOWN_RESPONSE) {}
	};

	// Master -> dashboard (SHUTDOWN_RESPONSE): a world is gone
	struct WorldShutDown : public LUBitStream {
		LWOMAPID zoneID{};
		LWOINSTANCEID instanceID{};

		WorldShutDown() : LUBitStream(ServiceType::MASTER, MessageType::Master::SHUTDOWN_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// World -> master: a GM asked to shut every server down in 10 minutes
	struct ShutdownUniverse : public LUBitStream {
		ShutdownUniverse() : LUBitStream(ServiceType::MASTER, MessageType::Master::SHUTDOWN_UNIVERSE) {}
	};

	// Master -> world: can you take the player of requestID? (AFFIRM_TRANSFER_REQUEST); world -> master: yes
	// (AFFIRM_TRANSFER_RESPONSE)
	struct AffirmTransfer : public LUBitStream {
		uint64_t requestID{};

		AffirmTransfer(MessageType::Master id) : LUBitStream(ServiceType::MASTER, id) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct AffirmTransferRequest : public AffirmTransfer {
		AffirmTransferRequest() : AffirmTransfer(MessageType::Master::AFFIRM_TRANSFER_REQUEST) {}
	};

	struct AffirmTransferResponse : public AffirmTransfer {
		AffirmTransferResponse() : AffirmTransfer(MessageType::Master::AFFIRM_TRANSFER_RESPONSE) {}
	};

	// Dashboard -> master: which servers are running (answered with ServerListResponse)
	struct RequestServerList : public LUBitStream {
		RequestServerList() : LUBitStream(ServiceType::MASTER, MessageType::Master::REQUEST_SERVER_LIST) {}
	};

	// Master -> dashboard
	struct ServerListResponse : public LUBitStream {
		// More than this many worlds is a broken packet
		static constexpr uint32_t MAX_INSTANCES = 10000;

		// Where a world is: connected, launched but not connected yet, shutting down, or connected but having its
		// players moved to a new instance (a live update or instance migration; nobody new is sent there)
		enum class eState : uint8_t { READY = 0, STARTING = 1, STOPPING = 2, DRAINING = 3 };

		struct Instance {
			LWOMAPID mapID{};
			LWOINSTANCEID instanceID{};
			LWOCLONEID cloneID{};
			uint32_t players{};
			LUString ip{};
			uint32_t port{};
			uint8_t isPrivate{};
			eState state{ eState::READY }; // written after the UGC fields, one byte per instance
		};

		uint8_t authOnline{};
		uint8_t chatOnline{};
		std::vector<Instance> instances; // u32 count
		// The UGC server (docs/UgcServer.md), after the instances: whether master starts it (enable_ugc_server),
		// whether it is connected, and the process id master last started it as (0: unknown)
		uint8_t ugcEnabled{};
		uint8_t ugcOnline{};
		uint32_t ugcPid{};

		// Where each server listens and which machine it runs on, after the worlds' states: master, auth, chat, the
		// dashboard, the UGC server and every world. `ip` and `port` are what the server said it listens on (the
		// dashboard and the UGC server: their web port); `host` is its machine, the address master sees its
		// connection come from (master's own machine, and servers connecting from it, are master's external_ip)
		struct Endpoint {
			ServiceType type{};
			uint32_t zoneID{};
			uint32_t instanceID{};
			LUString ip{};
			uint32_t port{};
			LUString host{};
		};
		std::vector<Endpoint> endpoints; // u32 count, at most MAX_INSTANCES + MAX_OTHER_ENDPOINTS
		static constexpr uint32_t MAX_OTHER_ENDPOINTS = 16;

		ServerListResponse() : LUBitStream(ServiceType::MASTER, MessageType::Master::SERVER_LIST_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Dashboard -> master: shut the whole server down now (scheduled restarts)
	struct DashboardShutdown : public LUBitStream {
		DashboardShutdown() : LUBitStream(ServiceType::MASTER, MessageType::Master::DASHBOARD_SHUTDOWN) {}
	};

	// Dashboard -> master -> every server: settings changed on the dashboard, reload the config
	struct ConfigReload : public LUBitStream {
		ConfigReload() : LUBitStream(ServiceType::MASTER, MessageType::Master::CONFIG_RELOAD) {}
	};

	// Dashboard -> master: shut down one world instance
	struct InstanceShutdown : public LUBitStream {
		uint32_t zoneID{};
		uint32_t instanceID{};

		InstanceShutdown() : LUBitStream(ServiceType::MASTER, MessageType::Master::INSTANCE_SHUTDOWN) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};
}

#endif // MASTERPACKETS_H
