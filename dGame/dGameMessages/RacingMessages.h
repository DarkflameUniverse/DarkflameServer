#ifndef RACINGMESSAGES_H
#define RACINGMESSAGES_H

#include "GameMessages.h"
#include "NiPoint3.h"
#include "eRacingClientNotificationType.h"

#include <string>

// Game messages for racing, vehicles and modular assembly (car building).
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
namespace GameMessages {
	// Server -> client.
	struct ModuleAssemblyDBDataForClient : public NetGameMsg {
		ModuleAssemblyDBDataForClient() : NetGameMsg(MessageType::Game::MODULE_ASSEMBLY_DB_DATA_FOR_CLIENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID assemblyID{};
		std::u16string blob{};
	};

	// Client -> server. No payload.
	struct ModuleAssemblyQueryData : public NetGameMsg {
		ModuleAssemblyQueryData() : NetGameMsg(MessageType::Game::MODULE_ASSEMBLY_QUERY_DATA) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Client -> server.
	struct ModularAssemblyNIFCompleted : public NetGameMsg {
		ModularAssemblyNIFCompleted() : NetGameMsg(MessageType::Game::MODULAR_ASSEMBLY_NIF_COMPLETED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID objectID{};
	};

	// Server -> client.
	struct NotifyVehicleOfRacingObject : public NetGameMsg {
		NotifyVehicleOfRacingObject() : NetGameMsg(MessageType::Game::NOTIFY_VEHICLE_OF_RACING_OBJECT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID racingObjectID{ LWOOBJID_EMPTY }; // optional
	};

	// Server -> client.
	struct RacingPlayerLoaded : public NetGameMsg {
		RacingPlayerLoaded() : NetGameMsg(MessageType::Game::RACING_PLAYER_LOADED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID playerID{};
		LWOOBJID vehicleID{};
	};

	// Server -> client.
	struct VehicleUnlockInput : public NetGameMsg {
		VehicleUnlockInput() : NetGameMsg(MessageType::Game::VEHICLE_UNLOCK_INPUT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bLockWheels{};
	};

	// Both directions. DLU ignores it when received.
	struct VehicleSetWheelLockState : public NetGameMsg {
		VehicleSetWheelLockState() : NetGameMsg(MessageType::Game::VEHICLE_SET_WHEEL_LOCK_STATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bExtraFriction{};
		bool bLocked{};
	};

	// Server -> client.
	struct RacingSetPlayerResetInfo : public NetGameMsg {
		RacingSetPlayerResetInfo() : NetGameMsg(MessageType::Game::RACING_SET_PLAYER_RESET_INFO) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t currentLap{};
		uint32_t furthestResetPlane{};
		LWOOBJID playerID{};
		NiPoint3 respawnPos{};
		uint32_t upcomingPlane{};
	};

	// Server -> client.
	struct RacingResetPlayerToLastReset : public NetGameMsg {
		RacingResetPlayerToLastReset() : NetGameMsg(MessageType::Game::RACING_RESET_PLAYER_TO_LAST_RESET) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID playerID{};
	};

	// Server -> client.
	struct VehicleStopBoost : public NetGameMsg {
		VehicleStopBoost() : NetGameMsg(MessageType::Game::VEHICLE_STOP_BOOST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bAffectPassive{};
	};

	// Server -> client.
	struct NotifyRacingClient : public NetGameMsg {
		NotifyRacingClient() : NetGameMsg(MessageType::Game::NOTIFY_RACING_CLIENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eRacingClientNotificationType eventType{ eRacingClientNotificationType::INVALID }; // optional
		int32_t param1{};
		LWOOBJID paramObj{};
		std::u16string paramStr{};
		LWOOBJID singleClient{};
	};

	// Server -> client. No payload.
	struct VehicleAddPassiveBoostAction : public NetGameMsg {
		VehicleAddPassiveBoostAction() : NetGameMsg(MessageType::Game::VEHICLE_ADD_PASSIVE_BOOST_ACTION) {}
	};

	// Server -> client. No payload.
	struct VehicleRemovePassiveBoostAction : public NetGameMsg {
		VehicleRemovePassiveBoostAction() : NetGameMsg(MessageType::Game::VEHICLE_REMOVE_PASSIVE_BOOST_ACTION) {}
	};

	// Server -> client. No payload.
	struct VehicleNotifyFinishedRace : public NetGameMsg {
		VehicleNotifyFinishedRace() : NetGameMsg(MessageType::Game::VEHICLE_NOTIFY_FINISHED_RACE) {}
	};

	// Client -> server. No payload. DLU ignores it.
	struct VehicleNotifyServerAddPassiveBoostAction : public NetGameMsg {
		VehicleNotifyServerAddPassiveBoostAction() : NetGameMsg(MessageType::Game::NOTIFY_SERVER_VEHICLE_ADD_PASSIVE_BOOST_ACTION) {}
	};

	// Client -> server. No payload. DLU ignores it.
	struct VehicleNotifyServerRemovePassiveBoostAction : public NetGameMsg {
		VehicleNotifyServerRemovePassiveBoostAction() : NetGameMsg(MessageType::Game::NOTIFY_SERVER_VEHICLE_REMOVE_PASSIVE_BOOST_ACTION) {}
	};

	// Client -> server.
	struct RacingClientReady : public NetGameMsg {
		RacingClientReady() : NetGameMsg(MessageType::Game::RACING_CLIENT_READY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID playerID{};
	};

	// Client -> server.
	struct RacingPlayerInfoResetFinished : public NetGameMsg {
		RacingPlayerInfoResetFinished() : NetGameMsg(MessageType::Game::RACING_PLAYER_INFO_RESET_FINISHED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID playerID{};
	};

	// Client -> server. Sent when a vehicle drives through an imagination pickup.
	struct VehicleNotifyHitImaginationServer : public NetGameMsg {
		VehicleNotifyHitImaginationServer() : NetGameMsg(MessageType::Game::VEHICLE_NOTIFY_HIT_IMAGINATION_SERVER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID pickupObjID{ LWOOBJID_EMPTY }; // optional
		LWOOBJID pickupSpawnerID{ LWOOBJID_EMPTY }; // optional
		int32_t pickupSpawnerIndex{ -1 }; // optional
		NiPoint3 vehiclePosition{ NiPoint3Constant::ZERO }; // optional
	};
};

#endif // RACINGMESSAGES_H
