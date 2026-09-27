#include "MovementMessages.h"

#include "BitStreamUtils.h"
#include "CppScripts.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "GhostComponent.h"
#include "PlayerManager.h"
#include "PossessorComponent.h"
#include "RailActivatorComponent.h"
#include "eReplicaComponentType.h"

namespace GameMessages {
	namespace {
		void WritePoint(RakNet::BitStream& bitStream, const NiPoint3& point) {
			bitStream.Write(point.x);
			bitStream.Write(point.y);
			bitStream.Write(point.z);
		}

		bool ReadPoint(RakNet::BitStream& bitStream, NiPoint3& point) {
			return bitStream.Read(point.x) && bitStream.Read(point.y) && bitStream.Read(point.z);
		}
	}

	void Teleport::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bIgnoreY);
		bitStream.Write(bSetRotation);
		bitStream.Write(bSkipAllChecks);
		WritePoint(bitStream, pos);
		bitStream.Write(bUseNavmesh);
		BitStreamUtils::WriteOptional(bitStream, rot.w, 1.0f);
		bitStream.Write(rot.x);
		bitStream.Write(rot.y);
		bitStream.Write(rot.z);
	}

	bool Teleport::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bIgnoreY));
		VALIDATE_READ(bitStream.Read(bSetRotation));
		VALIDATE_READ(bitStream.Read(bSkipAllChecks));
		VALIDATE_READ(ReadPoint(bitStream, pos));
		VALIDATE_READ(bitStream.Read(bUseNavmesh));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, rot.w, 1.0f));
		VALIDATE_READ(bitStream.Read(rot.x));
		VALIDATE_READ(bitStream.Read(rot.y));
		VALIDATE_READ(bitStream.Read(rot.z));
		return true;
	}

	PlatformResync::PlatformResync(const Entity& entity, bool bStopAtDesiredWaypoint, int32_t iIndex, int32_t iDesiredWaypointIndex,
		int32_t nextIndex, eMovementPlatformState movementState, bool special) : PlatformResync() {
		const auto lot = entity.GetLOT();

		if (lot == 12341 || lot == 5027 || lot == 5028 || lot == 14335 || lot == 14447 || lot == 14449 || lot == 11306 || lot == 11308 || lot == 9483) {
			iDesiredWaypointIndex = (lot == 11306 || lot == 11308) ? 1 : 0;
			iIndex = lot == 9483 ? 1 : 0;
			nextIndex = lot == 9483 && !special ? 1 : 0;
			bStopAtDesiredWaypoint = true;
			movementState = lot == 9483 && !special ? eMovementPlatformState::Stopped : eMovementPlatformState::Stationary;
		}

		target = entity.GetObjectID();
		this->bStopAtDesiredWaypoint = bStopAtDesiredWaypoint;
		this->iIndex = iIndex;
		this->iDesiredWaypointIndex = iDesiredWaypointIndex;
		this->iNextIndex = nextIndex;
		this->eState = movementState;
	}

	void PlatformResync::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bReverse);
		bitStream.Write(bStopAtDesiredWaypoint);
		bitStream.Write(eCommand);
		bitStream.Write(static_cast<int32_t>(eState));
		bitStream.Write(eUnexpectedCommand);
		bitStream.Write(fIdleTimeElapsed);
		bitStream.Write(fMoveTimeElapsed);
		bitStream.Write(fPercentBetweenPoints);
		bitStream.Write(iDesiredWaypointIndex);
		bitStream.Write(iIndex);
		bitStream.Write(iNextIndex);
		WritePoint(bitStream, ptUnexpectedLocation);
		const bool hasRotation = qUnexpectedRotation != QuatUtils::IDENTITY;
		bitStream.Write(hasRotation);
		if (hasRotation) {
			bitStream.Write(qUnexpectedRotation.x);
			bitStream.Write(qUnexpectedRotation.y);
			bitStream.Write(qUnexpectedRotation.z);
			bitStream.Write(qUnexpectedRotation.w);
		}
	}

	bool PlatformResync::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bReverse));
		VALIDATE_READ(bitStream.Read(bStopAtDesiredWaypoint));
		VALIDATE_READ(bitStream.Read(eCommand));
		int32_t state{};
		VALIDATE_READ(bitStream.Read(state));
		eState = static_cast<eMovementPlatformState>(state);
		VALIDATE_READ(bitStream.Read(eUnexpectedCommand));
		VALIDATE_READ(bitStream.Read(fIdleTimeElapsed));
		VALIDATE_READ(bitStream.Read(fMoveTimeElapsed));
		VALIDATE_READ(bitStream.Read(fPercentBetweenPoints));
		VALIDATE_READ(bitStream.Read(iDesiredWaypointIndex));
		VALIDATE_READ(bitStream.Read(iIndex));
		VALIDATE_READ(bitStream.Read(iNextIndex));
		VALIDATE_READ(ReadPoint(bitStream, ptUnexpectedLocation));
		bool hasRotation{};
		VALIDATE_READ(bitStream.Read(hasRotation));
		qUnexpectedRotation = QuatUtils::IDENTITY;
		if (hasRotation) {
			VALIDATE_READ(bitStream.Read(qUnexpectedRotation.x));
			VALIDATE_READ(bitStream.Read(qUnexpectedRotation.y));
			VALIDATE_READ(bitStream.Read(qUnexpectedRotation.z));
			VALIDATE_READ(bitStream.Read(qUnexpectedRotation.w));
		}
		return true;
	}

	void RequestPlatformResync::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (entity.GetLOT() == 6267 || entity.GetLOT() == 16141) return;
		PlatformResync(entity).Send(sysAddr);
	}

	void OrientToAngle::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bRelativeToCurrent);
		bitStream.Write(fAngle);
	}

	bool OrientToAngle::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bRelativeToCurrent));
		VALIDATE_READ(bitStream.Read(fAngle));
		return true;
	}

	void LockNodeRotation::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, nodeName);
	}

	bool LockNodeRotation::Deserialize(RakNet::BitStream& bitStream) {
		return BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, nodeName);
	}

	void SetGravityScale::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(scale);
	}

	bool SetGravityScale::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(scale);
	}

	void SetJetPackMode::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bBypassChecks);
		bitStream.Write(bDoHover);
		bitStream.Write(bUse);
		BitStreamUtils::WriteOptional(bitStream, effectID, -1);
		BitStreamUtils::WriteOptional(bitStream, fAirspeed, 10.0f);
		BitStreamUtils::WriteOptional(bitStream, fMaxAirspeed, 15.0f);
		BitStreamUtils::WriteOptional(bitStream, fVertVel, 1.0f);
		BitStreamUtils::WriteOptional(bitStream, iWarningEffectID, -1);
	}

	bool SetJetPackMode::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bBypassChecks));
		VALIDATE_READ(bitStream.Read(bDoHover));
		VALIDATE_READ(bitStream.Read(bUse));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, effectID, -1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, fAirspeed, 10.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, fMaxAirspeed, 15.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, fVertVel, 1.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iWarningEffectID, -1));
		return true;
	}

	void SetPlayerControlScheme::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bDelayCamSwitchIfInCinematic);
		bitStream.Write(bSwitchCam);
		BitStreamUtils::WriteOptional(bitStream, iScheme, eControlScheme::SCHEME_A);
	}

	bool SetPlayerControlScheme::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bDelayCamSwitchIfInCinematic));
		VALIDATE_READ(bitStream.Read(bSwitchCam));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iScheme, eControlScheme::SCHEME_A));
		return true;
	}

	void PlayerReachedRespawnCheckpoint::Serialize(RakNet::BitStream& bitStream) const {
		WritePoint(bitStream, pos);
		const bool hasRotation = rot != QuatUtils::IDENTITY;
		bitStream.Write(hasRotation);
		if (hasRotation) {
			bitStream.Write(rot.w);
			bitStream.Write(rot.x);
			bitStream.Write(rot.y);
			bitStream.Write(rot.z);
		}
	}

	bool PlayerReachedRespawnCheckpoint::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(ReadPoint(bitStream, pos));
		bool hasRotation{};
		VALIDATE_READ(bitStream.Read(hasRotation));
		rot = QuatUtils::IDENTITY;
		if (hasRotation) {
			VALIDATE_READ(bitStream.Read(rot.w));
			VALIDATE_READ(bitStream.Read(rot.x));
			VALIDATE_READ(bitStream.Read(rot.y));
			VALIDATE_READ(bitStream.Read(rot.z));
		}
		return true;
	}

	void SetRailMovement::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(pathGoForward);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, pathName);
		bitStream.Write(pathStart);
		BitStreamUtils::WriteOptional(bitStream, railActivatorComponentID, -1);
		BitStreamUtils::WriteOptional(bitStream, railActivatorObjectID, LWOOBJID_EMPTY);
	}

	bool SetRailMovement::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(pathGoForward));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, pathName));
		VALIDATE_READ(bitStream.Read(pathStart));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, railActivatorComponentID, -1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, railActivatorObjectID, LWOOBJID_EMPTY));
		return true;
	}

	void StartRailMovement::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bDamageImmune);
		bitStream.Write(bNoAggro);
		bitStream.Write(bNotifyActor);
		bitStream.Write(bShowNameBillboard);
		bitStream.Write(bCameraLocked);
		bitStream.Write(bCollisionEnabled);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, loopSound);
		bitStream.Write(goForward);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, pathName);
		BitStreamUtils::WriteOptional<uint32_t>(bitStream, pathStart, 0);
		BitStreamUtils::WriteOptional(bitStream, railActivatorComponentID, -1);
		BitStreamUtils::WriteOptional(bitStream, railActivatorObjectID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, startSound);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, stopSound);
		bitStream.Write(bUseDB);
	}

	bool StartRailMovement::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bDamageImmune));
		VALIDATE_READ(bitStream.Read(bNoAggro));
		VALIDATE_READ(bitStream.Read(bNotifyActor));
		VALIDATE_READ(bitStream.Read(bShowNameBillboard));
		VALIDATE_READ(bitStream.Read(bCameraLocked));
		VALIDATE_READ(bitStream.Read(bCollisionEnabled));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, loopSound));
		VALIDATE_READ(bitStream.Read(goForward));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, pathName));
		VALIDATE_READ(BitStreamUtils::ReadOptional<uint32_t>(bitStream, pathStart, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, railActivatorComponentID, -1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, railActivatorObjectID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, startSound));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, stopSound));
		VALIDATE_READ(bitStream.Read(bUseDB));
		return true;
	}

	void ClientRailMovementReady::Handle(Entity& entity, const SystemAddress& sysAddr) {
		const auto possibleRails = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RAIL_ACTIVATOR);
		for (const auto* possibleRail : possibleRails) {
			const auto* rail = possibleRail->GetComponent<RailActivatorComponent>();
			if (rail != nullptr) {
				rail->OnRailMovementReady(&entity);
			}
		}
	}

	void CancelRailMovement::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bImmediate);
	}

	bool CancelRailMovement::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(bImmediate);
	}

	void CancelRailMovement::Handle(Entity& entity, const SystemAddress& sysAddr) {
		const auto possibleRails = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RAIL_ACTIVATOR);
		for (const auto* possibleRail : possibleRails) {
			auto* rail = possibleRail->GetComponent<RailActivatorComponent>();
			if (rail != nullptr) {
				rail->OnCancelRailMovement(&entity);
			}
		}
	}

	void PlayerRailArrivedNotification::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, pathName);
		bitStream.Write(waypointNumber);
	}

	bool PlayerRailArrivedNotification::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, pathName));
		VALIDATE_READ(bitStream.Read(waypointNumber));
		return true;
	}

	void PlayerRailArrivedNotification::Handle(Entity& entity, const SystemAddress& sysAddr) {
		const auto possibleRails = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RAIL_ACTIVATOR);
		for (auto* possibleRail : possibleRails) {
			if (possibleRail) possibleRail->GetScript()->OnPlayerRailArrived(possibleRail, &entity, pathName, waypointNumber);
		}
	}

	void SetMountInventoryID::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, inventoryMountID, LWOOBJID_EMPTY);
	}

	bool SetMountInventoryID::Deserialize(RakNet::BitStream& bitStream) {
		return BitStreamUtils::ReadOptional(bitStream, inventoryMountID, LWOOBJID_EMPTY);
	}

	void DismountComplete::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(mountID);
	}

	bool DismountComplete::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(mountID);
	}

	void DismountComplete::Handle(Entity& entity, const SystemAddress& sysAddr) {
		// If we aren't possessing something, don't do anything
		if (mountID == LWOOBJID_EMPTY) return;
		auto* possessorComponent = entity.GetComponent<PossessorComponent>();
		if (possessorComponent) possessorComponent->OnDismountComplete(mountID);
	}

	void AcknowledgePossession::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, possessedObjID, LWOOBJID_EMPTY);
	}

	bool AcknowledgePossession::Deserialize(RakNet::BitStream& bitStream) {
		return BitStreamUtils::ReadOptional(bitStream, possessedObjID, LWOOBJID_EMPTY);
	}

	void AcknowledgePossession::Handle(Entity& entity, const SystemAddress& sysAddr) {
		Game::entityManager->SerializeEntity(&entity);
		if (possessedObjID != LWOOBJID_EMPTY) {
			auto* mount = Game::entityManager->GetEntity(possessedObjID);
			if (mount) Game::entityManager->SerializeEntity(mount);
		}
	}

	void ToggleGhostReferenceOverride::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bOverride);
	}

	bool ToggleGhostReferenceOverride::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(bOverride);
	}

	void ToggleGhostReferenceOverride::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* player = PlayerManager::GetPlayer(sysAddr);
		if (player == nullptr) return;

		auto* ghostComponent = entity.GetComponent<GhostComponent>();
		if (ghostComponent) ghostComponent->SetGhostOverride(bOverride);

		Game::entityManager->UpdateGhosting(player);
	}

	void SetGhostReferencePosition::Serialize(RakNet::BitStream& bitStream) const {
		WritePoint(bitStream, pos);
	}

	bool SetGhostReferencePosition::Deserialize(RakNet::BitStream& bitStream) {
		return ReadPoint(bitStream, pos);
	}

	void SetGhostReferencePosition::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* player = PlayerManager::GetPlayer(sysAddr);
		if (player == nullptr) return;

		auto* ghostComponent = entity.GetComponent<GhostComponent>();
		if (ghostComponent) ghostComponent->SetGhostOverridePoint(pos);

		Game::entityManager->UpdateGhosting(player);
	}

	void PlayerSetCameraCyclingMode::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bAllowCyclingWhileDeadOnly);
		BitStreamUtils::WriteOptional(bitStream, cyclingMode, eCyclingMode::ALLOW_CYCLE_TEAMMATES);
	}

	bool PlayerSetCameraCyclingMode::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bAllowCyclingWhileDeadOnly));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, cyclingMode, eCyclingMode::ALLOW_CYCLE_TEAMMATES));
		return true;
	}

	void ForceCameraTargetCycle::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bForceCycling);
		BitStreamUtils::WriteOptional(bitStream, cyclingMode, eCameraTargetCyclingMode::ALLOW_CYCLE_TEAMMATES);
		bitStream.Write(optionalTargetID);
	}

	bool ForceCameraTargetCycle::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bForceCycling));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, cyclingMode, eCameraTargetCyclingMode::ALLOW_CYCLE_TEAMMATES));
		VALIDATE_READ(bitStream.Read(optionalTargetID));
		return true;
	}
}
