#ifndef RACINGMESSAGESLEGACY_H
#define RACINGMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written GameMessages::Send* functions that RacingMessages.h replaced
// (dGame/dGameMessages/GameMessages.cpp, branched from origin/main 129199e4). Only the namespace changed.
// The Read* functions are the read sequences of the replaced GameMessages::Handle* functions, verbatim up to
// the point where the handler starts using what it read.

#include "LegacyPacketMacros.h"
#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "Entity.h"
#include "Game.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "NiPoint3.h"
#include "ServiceType.h"

#include <string>

namespace LegacyGameMessages {
	inline void SendModuleAssemblyDBDataForClient(LWOOBJID objectId, LWOOBJID assemblyID, const std::u16string& data, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::MODULE_ASSEMBLY_DB_DATA_FOR_CLIENT);

		bitStream.Write(assemblyID);

		bitStream.Write<uint32_t>(data.size());
		for (auto character : data) {
			bitStream.Write(character);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendNotifyVehicleOfRacingObject(LWOOBJID objectId, LWOOBJID racingObjectID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::NOTIFY_VEHICLE_OF_RACING_OBJECT);

		bitStream.Write(racingObjectID != LWOOBJID_EMPTY);
		if (racingObjectID != LWOOBJID_EMPTY) bitStream.Write(racingObjectID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendRacingPlayerLoaded(LWOOBJID objectId, LWOOBJID playerID, LWOOBJID vehicleID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::RACING_PLAYER_LOADED);

		bitStream.Write(playerID);
		bitStream.Write(vehicleID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendVehicleUnlockInput(LWOOBJID objectId, bool bLockWheels, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::VEHICLE_UNLOCK_INPUT);

		bitStream.Write(bLockWheels);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendVehicleSetWheelLockState(LWOOBJID objectId, bool bExtraFriction, bool bLocked, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::VEHICLE_SET_WHEEL_LOCK_STATE);

		bitStream.Write(bExtraFriction);
		bitStream.Write(bLocked);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendRacingSetPlayerResetInfo(LWOOBJID objectId, int32_t currentLap, uint32_t furthestResetPlane, LWOOBJID playerID, NiPoint3 respawnPos, uint32_t upcomingPlane, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::RACING_SET_PLAYER_RESET_INFO);

		bitStream.Write(currentLap);
		bitStream.Write(furthestResetPlane);
		bitStream.Write(playerID);
		bitStream.Write(respawnPos);
		bitStream.Write(upcomingPlane);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendRacingResetPlayerToLastReset(LWOOBJID objectId, LWOOBJID playerID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::RACING_RESET_PLAYER_TO_LAST_RESET);

		bitStream.Write(playerID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendVehicleStopBoost(Entity* targetEntity, const SystemAddress& playerSysAddr, bool affectPassive) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(targetEntity->GetObjectID());
		bitStream.Write(MessageType::Game::VEHICLE_STOP_BOOST);

		bitStream.Write(affectPassive);

		SEND_PACKET_BROADCAST;
	}

	inline void SendNotifyRacingClient(LWOOBJID objectId, int32_t eventType, int32_t param1, LWOOBJID paramObj, std::u16string paramStr, LWOOBJID singleClient, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::NOTIFY_RACING_CLIENT);

		bitStream.Write(eventType != 0);
		if (eventType != 0) bitStream.Write(eventType);

		bitStream.Write(param1);

		bitStream.Write(paramObj);

		bitStream.Write<uint32_t>(paramStr.size());
		for (auto character : paramStr) {
			bitStream.Write(character);
		}

		bitStream.Write(singleClient);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendVehicleAddPassiveBoostAction(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::VEHICLE_ADD_PASSIVE_BOOST_ACTION);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendVehicleRemovePassiveBoostAction(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::VEHICLE_REMOVE_PASSIVE_BOOST_ACTION);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendVehicleNotifyFinishedRace(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::VEHICLE_NOTIFY_FINISHED_RACE);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	// GameMessages::HandleModularAssemblyNIFCompleted
	inline LWOOBJID ReadModularAssemblyNIFCompleted(RakNet::BitStream& inStream) {
		LWOOBJID objectID;

		inStream.Read(objectID);
		return objectID;
	}

	// GameMessages::HandleVehicleSetWheelLockState
	inline std::pair<bool, bool> ReadVehicleSetWheelLockState(RakNet::BitStream& inStream) {
		bool bExtraFriction = inStream.ReadBit();
		bool bLocked = inStream.ReadBit();
		return { bExtraFriction, bLocked };
	}

	// GameMessages::HandleRacingClientReady and GameMessages::HandleRacingPlayerInfoResetFinished
	inline LWOOBJID ReadRacingPlayerID(RakNet::BitStream& inStream) {
		LWOOBJID playerID;

		inStream.Read(playerID);
		return playerID;
	}

	// GameMessages::HandleVehicleNotifyHitImaginationServer
	struct LegacyHitImagination {
		LWOOBJID pickupObjID = LWOOBJID_EMPTY;
		LWOOBJID pickupSpawnerID = LWOOBJID_EMPTY;
		int32_t pickupSpawnerIndex = -1;
		NiPoint3 vehiclePosition = NiPoint3Constant::ZERO;
	};
	inline LegacyHitImagination ReadVehicleNotifyHitImaginationServer(RakNet::BitStream& inStream) {
		LWOOBJID pickupObjID = LWOOBJID_EMPTY;
		LWOOBJID pickupSpawnerID = LWOOBJID_EMPTY;
		int32_t pickupSpawnerIndex = -1;
		NiPoint3 vehiclePosition = NiPoint3Constant::ZERO;

		if (inStream.ReadBit()) inStream.Read(pickupObjID);
		if (inStream.ReadBit()) inStream.Read(pickupSpawnerID);
		if (inStream.ReadBit()) inStream.Read(pickupSpawnerIndex);
		if (inStream.ReadBit()) inStream.Read(vehiclePosition);
		return { pickupObjID, pickupSpawnerID, pickupSpawnerIndex, vehiclePosition };
	}
}

#endif // RACINGMESSAGESLEGACY_H
