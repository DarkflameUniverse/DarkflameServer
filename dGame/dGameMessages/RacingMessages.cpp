#include "RacingMessages.h"

#include "BitStreamUtils.h"
#include "CharacterComponent.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "ModuleAssemblyComponent.h"
#include "PossessableComponent.h"
#include "RacingControlComponent.h"
#include "dZoneManager.h"

namespace GameMessages {
	void ModuleAssemblyDBDataForClient::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(assemblyID);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, blob);
	}

	bool ModuleAssemblyDBDataForClient::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(assemblyID));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, blob));
		return true;
	}

	void ModuleAssemblyQueryData::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* moduleAssemblyComponent = entity.GetComponent<ModuleAssemblyComponent>();

		LOG("Got Query from %i", entity.GetLOT());

		if (moduleAssemblyComponent != nullptr) {
			LOG("Returning assembly %s", GeneralUtils::UTF16ToWTF8(moduleAssemblyComponent->GetAssemblyPartsLOTs()).c_str());

			ModuleAssemblyDBDataForClient response;
			response.target = entity.GetObjectID();
			response.assemblyID = moduleAssemblyComponent->GetSubKey();
			response.blob = moduleAssemblyComponent->GetAssemblyPartsLOTs();
			response.Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
	}

	void ModularAssemblyNIFCompleted::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(objectID);
	}

	bool ModularAssemblyNIFCompleted::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(objectID));
		return true;
	}

	void NotifyVehicleOfRacingObject::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, racingObjectID, LWOOBJID_EMPTY);
	}

	bool NotifyVehicleOfRacingObject::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, racingObjectID, LWOOBJID_EMPTY));
		return true;
	}

	void RacingPlayerLoaded::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
		bitStream.Write(vehicleID);
	}

	bool RacingPlayerLoaded::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		VALIDATE_READ(bitStream.Read(vehicleID));
		return true;
	}

	void VehicleUnlockInput::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bLockWheels);
	}

	bool VehicleUnlockInput::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bLockWheels));
		return true;
	}

	void VehicleSetWheelLockState::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bExtraFriction);
		bitStream.Write(bLocked);
	}

	bool VehicleSetWheelLockState::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bExtraFriction));
		VALIDATE_READ(bitStream.Read(bLocked));
		return true;
	}

	void RacingSetPlayerResetInfo::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(currentLap);
		bitStream.Write(furthestResetPlane);
		bitStream.Write(playerID);
		bitStream.Write(respawnPos);
		bitStream.Write(upcomingPlane);
	}

	bool RacingSetPlayerResetInfo::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(currentLap));
		VALIDATE_READ(bitStream.Read(furthestResetPlane));
		VALIDATE_READ(bitStream.Read(playerID));
		VALIDATE_READ(bitStream.Read(respawnPos));
		VALIDATE_READ(bitStream.Read(upcomingPlane));
		return true;
	}

	void RacingResetPlayerToLastReset::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool RacingResetPlayerToLastReset::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void VehicleStopBoost::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bAffectPassive);
	}

	bool VehicleStopBoost::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bAffectPassive));
		return true;
	}

	void NotifyRacingClient::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, eventType, eRacingClientNotificationType::INVALID);
		bitStream.Write(param1);
		bitStream.Write(paramObj);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, paramStr);
		bitStream.Write(singleClient);
	}

	bool NotifyRacingClient::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, eventType, eRacingClientNotificationType::INVALID));
		VALIDATE_READ(bitStream.Read(param1));
		VALIDATE_READ(bitStream.Read(paramObj));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, paramStr));
		VALIDATE_READ(bitStream.Read(singleClient));
		return true;
	}

	void RacingClientReady::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool RacingClientReady::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void RacingClientReady::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* player = Game::entityManager->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		auto* racingControlComponent = Game::zoneManager->GetZoneControlObject()->GetComponent<RacingControlComponent>();

		if (racingControlComponent == nullptr) {
			return;
		}

		racingControlComponent->OnRacingClientReady(player);
	}

	void RacingPlayerInfoResetFinished::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool RacingPlayerInfoResetFinished::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void RacingPlayerInfoResetFinished::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* player = Game::entityManager->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		auto* zoneController = Game::zoneManager->GetZoneControlObject();

		auto* racingControlComponent = zoneController->GetComponent<RacingControlComponent>();

		LOG("Got finished: %i", entity.GetLOT());

		if (racingControlComponent != nullptr) {
			racingControlComponent->OnRacingPlayerInfoResetFinished(player);
		}
	}

	void VehicleNotifyHitImaginationServer::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, pickupObjID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, pickupSpawnerID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, pickupSpawnerIndex, -1);
		BitStreamUtils::WriteOptional(bitStream, vehiclePosition, NiPoint3Constant::ZERO);
	}

	bool VehicleNotifyHitImaginationServer::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, pickupObjID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, pickupSpawnerID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, pickupSpawnerIndex, -1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, vehiclePosition, NiPoint3Constant::ZERO));
		return true;
	}

	void VehicleNotifyHitImaginationServer::Handle(Entity& vehicle, const SystemAddress& sysAddr) {
		auto* pickup = Game::entityManager->GetEntity(pickupObjID);

		if (pickup == nullptr) {
			return;
		}

		// The sender is the vehicle; credit its driver if it has one.
		Entity* entity = &vehicle;
		auto* possessableComponent = entity->GetComponent<PossessableComponent>();

		if (possessableComponent != nullptr) {
			entity = Game::entityManager->GetEntity(possessableComponent->GetPossessor());

			if (entity == nullptr) {
				return;
			}
		}

		auto* characterComponent = entity->GetComponent<CharacterComponent>();
		if (characterComponent != nullptr) {
			characterComponent->UpdatePlayerStatistic(RacingImaginationPowerUpsCollected);
		}

		pickup->OnFireEventServerSide(entity, "powerup");

		pickup->Kill(entity);
	}
}
