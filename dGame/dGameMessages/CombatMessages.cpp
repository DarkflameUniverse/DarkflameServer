#include "CombatMessages.h"

#include "BitStreamUtils.h"
#include "ControllablePhysicsComponent.h"
#include "CppScripts.h"
#include "DestroyableComponent.h"
#include "eBubbleType.h"
#include "eKillType.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
#include "PossessableComponent.h"
#include "RacingControlComponent.h"
#include "dZoneManager.h"

namespace GameMessages {
	void Die::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bClientDeath);
		bitStream.Write(bSpawnLoot);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, deathType);
		bitStream.Write(directionRelative_AngleXZ);
		bitStream.Write(directionRelative_AngleY);
		bitStream.Write(directionRelative_Force);
		BitStreamUtils::WriteOptional(bitStream, killType, eKillType::VIOLENT);
		bitStream.Write(killerID);
		BitStreamUtils::WriteOptional(bitStream, lootOwnerID, LWOOBJID_EMPTY);
	}

	bool Die::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bClientDeath));
		VALIDATE_READ(bitStream.Read(bSpawnLoot));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, deathType));
		VALIDATE_READ(bitStream.Read(directionRelative_AngleXZ));
		VALIDATE_READ(bitStream.Read(directionRelative_AngleY));
		VALIDATE_READ(bitStream.Read(directionRelative_Force));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, killType, eKillType::VIOLENT));
		VALIDATE_READ(bitStream.Read(killerID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, lootOwnerID, LWOOBJID_EMPTY));
		return true;
	}

	void RequestDie::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bClientDeath);
		bitStream.Write(bSpawnLoot);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, deathType);
		bitStream.Write(directionRelative_AngleXZ);
		bitStream.Write(directionRelative_AngleY);
		bitStream.Write(directionRelative_Force);
		BitStreamUtils::WriteOptional(bitStream, killType, eKillType::VIOLENT);
		bitStream.Write(killerID);
		BitStreamUtils::WriteOptional(bitStream, lootOwnerID, LWOOBJID_EMPTY);
	}

	bool RequestDie::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bClientDeath));
		VALIDATE_READ(bitStream.Read(bSpawnLoot));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, deathType));
		VALIDATE_READ(bitStream.Read(directionRelative_AngleXZ));
		VALIDATE_READ(bitStream.Read(directionRelative_AngleY));
		VALIDATE_READ(bitStream.Read(directionRelative_Force));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, killType, eKillType::VIOLENT));
		VALIDATE_READ(bitStream.Read(killerID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, lootOwnerID, LWOOBJID_EMPTY));
		return true;
	}

	void RequestDie::Handle(Entity& entityRef, const SystemAddress& sysAddr) {
		Entity* entity = &entityRef;
		auto* zoneController = Game::zoneManager->GetZoneControlObject();

		auto* racingControlComponent = zoneController->GetComponent<RacingControlComponent>();

		LOG("Got die request: %i", entity->GetLOT());

		if (racingControlComponent != nullptr) {
			auto* possessableComponent = entity->GetComponent<PossessableComponent>();

			if (possessableComponent != nullptr) {
				entity = Game::entityManager->GetEntity(possessableComponent->GetPossessor());

				if (entity == nullptr) {
					return;
				}
			}

			racingControlComponent->OnRequestDie(entity);
		} else {
			auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

			if (!destroyableComponent) return;

			destroyableComponent->Smash(killerID, killType, deathType);
		}
	}

	void RequestSmashPlayer::Serialize(RakNet::BitStream& bitStream) const {}

	bool RequestSmashPlayer::Deserialize(RakNet::BitStream& bitStream) {
		return true;
	}

	void RequestSmashPlayer::Handle(Entity& entity, const SystemAddress& sysAddr) {
		entity.Smash(entity.GetObjectID());
	}

	void RequestResurrect::Serialize(RakNet::BitStream& bitStream) const {}

	bool RequestResurrect::Deserialize(RakNet::BitStream& bitStream) {
		return true;
	}

	void RequestResurrect::Handle(Entity& entity, const SystemAddress& sysAddr) {
		DestroyableComponent::Resurrect(entity);
	}

	void Resurrect::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bRezImmediately);
	}

	bool Resurrect::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bRezImmediately));
		return true;
	}

	void Resurrect::Handle(Entity& entityRef, const SystemAddress& sysAddr) {
		Entity* entity = &entityRef;
		Entity* zoneControl = Game::entityManager->GetZoneControlEntity();
		if (zoneControl) {
			zoneControl->GetScript()->OnPlayerResurrected(zoneControl, entity);
		}

		std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPTED_ACTIVITY);
		for (Entity* scriptEntity : scriptedActs) {
			if (scriptEntity->GetObjectID() != zoneControl->GetObjectID()) { // Don't want to trigger twice on instance worlds
				scriptEntity->GetScript()->OnPlayerResurrected(scriptEntity, entity);
			}
		}
	}

	void Smash::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bIgnoreObjectVisibility);
		bitStream.Write(force);
		bitStream.Write(ghostOpacity);
		bitStream.Write(killerID);
	}

	bool Smash::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bIgnoreObjectVisibility));
		VALIDATE_READ(bitStream.Read(force));
		VALIDATE_READ(bitStream.Read(ghostOpacity));
		VALIDATE_READ(bitStream.Read(killerID));
		return true;
	}

	void UnSmash::Serialize(RakNet::BitStream& bitStream) const {
		// Both fields are optional with a default, like the client's GameMessage::UnSmash::Serialize.
		BitStreamUtils::WriteOptional(bitStream, builderID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, duration, 3.0f);
	}

	bool UnSmash::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, builderID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, duration, 3.0f));
		return true;
	}

	void SetResurrectRestoreValues::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, iArmorRestore, -1);
		BitStreamUtils::WriteOptional(bitStream, iHealthRestore, -1);
		BitStreamUtils::WriteOptional(bitStream, iImaginationRestore, -1);
	}

	bool SetResurrectRestoreValues::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iArmorRestore, -1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iHealthRestore, -1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iImaginationRestore, -1));
		return true;
	}

	void SetPlayerAllowedRespawn::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(dontPromptForRespawn);
	}

	bool SetPlayerAllowedRespawn::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(dontPromptForRespawn));
		return true;
	}

	void Knockback::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, Caster, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, Originator, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, iKnockBackTimeMS, 0);
		bitStream.Write(vector);
	}

	bool Knockback::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, Caster, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, Originator, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iKnockBackTimeMS, 0));
		VALIDATE_READ(bitStream.Read(vector));
		return true;
	}

	void SetStunned::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, Originator, LWOOBJID_EMPTY);
		bitStream.Write(StateChangeType);
		bitStream.Write(bCantAttack);
		bitStream.Write(bCantAttackOutChangeWasApplied);
		bitStream.Write(bCantEquip);
		bitStream.Write(bCantEquipOutChangeWasApplied);
		bitStream.Write(bCantInteract);
		bitStream.Write(bCantInteractOutChangeWasApplied);
		bitStream.Write(bCantJump);
		bitStream.Write(bCantJumpOutChangeWasApplied);
		bitStream.Write(bCantMove);
		bitStream.Write(bCantMoveOutChangeWasApplied);
		bitStream.Write(bCantTurn);
		bitStream.Write(bCantTurnOutChangeWasApplied);
		bitStream.Write(bCantUseItem);
		bitStream.Write(bCantUseItemOutChangeWasApplied);
		bitStream.Write(bDontTerminateInteract);
		bitStream.Write(bIgnoreImmunity);
	}

	bool SetStunned::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, Originator, LWOOBJID_EMPTY));
		VALIDATE_READ(bitStream.Read(StateChangeType));
		VALIDATE_READ(bitStream.Read(bCantAttack));
		VALIDATE_READ(bitStream.Read(bCantAttackOutChangeWasApplied));
		VALIDATE_READ(bitStream.Read(bCantEquip));
		VALIDATE_READ(bitStream.Read(bCantEquipOutChangeWasApplied));
		VALIDATE_READ(bitStream.Read(bCantInteract));
		VALIDATE_READ(bitStream.Read(bCantInteractOutChangeWasApplied));
		VALIDATE_READ(bitStream.Read(bCantJump));
		VALIDATE_READ(bitStream.Read(bCantJumpOutChangeWasApplied));
		VALIDATE_READ(bitStream.Read(bCantMove));
		VALIDATE_READ(bitStream.Read(bCantMoveOutChangeWasApplied));
		VALIDATE_READ(bitStream.Read(bCantTurn));
		VALIDATE_READ(bitStream.Read(bCantTurnOutChangeWasApplied));
		VALIDATE_READ(bitStream.Read(bCantUseItem));
		VALIDATE_READ(bitStream.Read(bCantUseItemOutChangeWasApplied));
		VALIDATE_READ(bitStream.Read(bDontTerminateInteract));
		VALIDATE_READ(bitStream.Read(bIgnoreImmunity));
		return true;
	}

	void SetStunImmunity::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, Caster, LWOOBJID_EMPTY);
		bitStream.Write(StateChangeType);
		bitStream.Write(bImmuneToStunAttack);
		bitStream.Write(bImmuneToStunEquip);
		bitStream.Write(bImmuneToStunInteract);
		bitStream.Write(bImmuneToStunJump);
		bitStream.Write(bImmuneToStunMove);
		bitStream.Write(bImmuneToStunTurn);
		bitStream.Write(bImmuneToStunUseItem);
	}

	bool SetStunImmunity::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, Caster, LWOOBJID_EMPTY));
		VALIDATE_READ(bitStream.Read(StateChangeType));
		VALIDATE_READ(bitStream.Read(bImmuneToStunAttack));
		VALIDATE_READ(bitStream.Read(bImmuneToStunEquip));
		VALIDATE_READ(bitStream.Read(bImmuneToStunInteract));
		VALIDATE_READ(bitStream.Read(bImmuneToStunJump));
		VALIDATE_READ(bitStream.Read(bImmuneToStunMove));
		VALIDATE_READ(bitStream.Read(bImmuneToStunTurn));
		VALIDATE_READ(bitStream.Read(bImmuneToStunUseItem));
		return true;
	}

	void SetStatusImmunity::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(StateChangeType);
		bitStream.Write(bImmuneToBasicAttack);
		bitStream.Write(bImmuneToDOT);
		bitStream.Write(bImmuneToKnockback);
		bitStream.Write(bImmuneToInterrupt);
		bitStream.Write(bImmuneToSpeed);
		bitStream.Write(bImmuneToImaginationGain);
		bitStream.Write(bImmuneToImaginationLoss);
		bitStream.Write(bImmuneToQuickbuildInterrupt);
		bitStream.Write(bImmuneToPullToPoint);
	}

	bool SetStatusImmunity::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(StateChangeType));
		VALIDATE_READ(bitStream.Read(bImmuneToBasicAttack));
		VALIDATE_READ(bitStream.Read(bImmuneToDOT));
		VALIDATE_READ(bitStream.Read(bImmuneToKnockback));
		VALIDATE_READ(bitStream.Read(bImmuneToInterrupt));
		VALIDATE_READ(bitStream.Read(bImmuneToSpeed));
		VALIDATE_READ(bitStream.Read(bImmuneToImaginationGain));
		VALIDATE_READ(bitStream.Read(bImmuneToImaginationLoss));
		VALIDATE_READ(bitStream.Read(bImmuneToQuickbuildInterrupt));
		VALIDATE_READ(bitStream.Read(bImmuneToPullToPoint));
		return true;
	}

	void AddBuff::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bAddedByTeammate);
		bitStream.Write(bApplyOnTeammates);
		bitStream.Write(bCancelOnDamaged);
		bitStream.Write(bCancelOnDeath);
		bitStream.Write(bCancelOnLogOut);
		bitStream.Write(bCancelOnMove);
		bitStream.Write(bCancelOnRemoveBuff);
		bitStream.Write(bCancelOnUI);
		bitStream.Write(bCancelOnUnEquip);
		bitStream.Write(bCancelOnZone);
		bitStream.Write(bIgnoreImmunities);
		bitStream.Write(bIsImmunity);
		bitStream.Write(bUseRefCount);
		BitStreamUtils::WriteOptional(bitStream, i64AddedBy, LWOOBJID_EMPTY);
		bitStream.Write(uiBuffID);
		BitStreamUtils::WriteOptional(bitStream, uiDurationMS, 0u);
	}

	bool AddBuff::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bAddedByTeammate));
		VALIDATE_READ(bitStream.Read(bApplyOnTeammates));
		VALIDATE_READ(bitStream.Read(bCancelOnDamaged));
		VALIDATE_READ(bitStream.Read(bCancelOnDeath));
		VALIDATE_READ(bitStream.Read(bCancelOnLogOut));
		VALIDATE_READ(bitStream.Read(bCancelOnMove));
		VALIDATE_READ(bitStream.Read(bCancelOnRemoveBuff));
		VALIDATE_READ(bitStream.Read(bCancelOnUI));
		VALIDATE_READ(bitStream.Read(bCancelOnUnEquip));
		VALIDATE_READ(bitStream.Read(bCancelOnZone));
		VALIDATE_READ(bitStream.Read(bIgnoreImmunities));
		VALIDATE_READ(bitStream.Read(bIsImmunity));
		VALIDATE_READ(bitStream.Read(bUseRefCount));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, i64AddedBy, LWOOBJID_EMPTY));
		VALIDATE_READ(bitStream.Read(uiBuffID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, uiDurationMS, 0u));
		return true;
	}

	void RemoveBuff::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bFromRemoveBehavior);
		bitStream.Write(bFromUnEquip);
		bitStream.Write(bRemoveImmunity);
		bitStream.Write(uiBuffID);
	}

	bool RemoveBuff::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bFromRemoveBehavior));
		VALIDATE_READ(bitStream.Read(bFromUnEquip));
		VALIDATE_READ(bitStream.Read(bRemoveImmunity));
		VALIDATE_READ(bitStream.Read(uiBuffID));
		return true;
	}

	void AddRunSpeedModifier::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, i64Caster, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, uiModifier, 500u);
	}

	bool AddRunSpeedModifier::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, i64Caster, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, uiModifier, 500u));
		return true;
	}

	void RemoveRunSpeedModifier::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, uiModifier, 500u);
	}

	bool RemoveRunSpeedModifier::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, uiModifier, 500u));
		return true;
	}

	void ActivateBubbleBuff::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bSpecialAnims);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, wszType);
	}

	bool ActivateBubbleBuff::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bSpecialAnims));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, wszType));
		return true;
	}

	void ActivateBubbleBuff::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto bubbleType = eBubbleType::DEFAULT;
		if (wszType == u"skunk") bubbleType = eBubbleType::SKUNK;
		else if (wszType == u"energy") bubbleType = eBubbleType::ENERGY;

		auto controllablePhysicsComponent = entity.GetComponent<ControllablePhysicsComponent>();
		if (controllablePhysicsComponent) controllablePhysicsComponent->ActivateBubbleBuff(bubbleType, bSpecialAnims);
	}

	void DeactivateBubbleBuff::Serialize(RakNet::BitStream& bitStream) const {}

	bool DeactivateBubbleBuff::Deserialize(RakNet::BitStream& bitStream) {
		return true;
	}

	void DeactivateBubbleBuff::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto controllablePhysicsComponent = entity.GetComponent<ControllablePhysicsComponent>();
		if (controllablePhysicsComponent) controllablePhysicsComponent->DeactivateBubbleBuff();
		DeactivateBubbleBuffFromServer deactivate;
		deactivate.target = entity.GetObjectID();
		deactivate.Send(entity.GetSystemAddress());
	}

	void DeactivateBubbleBuffFromServer::Serialize(RakNet::BitStream& bitStream) const {}

	bool DeactivateBubbleBuffFromServer::Deserialize(RakNet::BitStream& bitStream) {
		return true;
	}
}
