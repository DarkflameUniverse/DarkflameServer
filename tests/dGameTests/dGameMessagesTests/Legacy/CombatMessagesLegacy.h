#ifndef COMBATMESSAGESLEGACY_H
#define COMBATMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written GameMessages functions that CombatMessages.h replaced
// (dGame/dGameMessages/GameMessages.cpp, branched from origin/main 129199e4). Only the namespace changed.
// The Read* functions are the read sequences of the replaced GameMessages::Handle* functions, verbatim up to
// the point where the handler starts using what it read.

#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "DestroyableComponent.h"
#include "eKillType.h"
#include "eStateChangeType.h"
#include "Entity.h"
#include "Game.h"
#include "GameMessages.h"
#include "GeneralUtils.h"
#include "LevelProgressionComponent.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "NiPoint3.h"
#include "ServiceType.h"

#include <string>

namespace LegacyGameMessages {
	inline void SendPlayerAllowedRespawn(LWOOBJID entityID, bool doNotPromptRespawn, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entityID);
		bitStream.Write(MessageType::Game::SET_PLAYER_ALLOWED_RESPAWN);
		bitStream.Write(doNotPromptRespawn);

		SEND_PACKET;
	}

	inline void SendKnockback(const LWOOBJID& objectID, const LWOOBJID& caster, const LWOOBJID& originator, int knockBackTimeMS, const NiPoint3& vector) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::KNOCKBACK);

		bool casterFlag = caster != LWOOBJID_EMPTY;
		bool originatorFlag = originator != LWOOBJID_EMPTY;
		bool knockBackTimeMSFlag = knockBackTimeMS != 0;

		bitStream.Write(casterFlag);
		if (casterFlag) bitStream.Write(caster);
		bitStream.Write(originatorFlag);
		if (originatorFlag) bitStream.Write(originator);
		bitStream.Write(knockBackTimeMSFlag);
		if (knockBackTimeMSFlag) bitStream.Write(knockBackTimeMS);
		bitStream.Write(vector);

		SEND_PACKET_BROADCAST;
	}

	inline void SendDieNoImplCode(Entity* entity, const LWOOBJID& killerID, const LWOOBJID& lootOwnerID, eKillType killType, std::u16string deathType, float directionRelative_AngleY, float directionRelative_AngleXZ, float directionRelative_Force, bool bClientDeath, bool bSpawnLoot) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::DIE);

		bitStream.Write(bClientDeath);
		bitStream.Write(bSpawnLoot);
		bitStream.Write<uint32_t>(deathType.size());
		bitStream.Write(deathType);
		bitStream.Write(directionRelative_AngleXZ);
		bitStream.Write(directionRelative_AngleY);
		bitStream.Write(directionRelative_Force);

		bitStream.Write(killType != eKillType::VIOLENT);
		if (killType != eKillType::VIOLENT) bitStream.Write(killType);

		bitStream.Write(killerID);
		bitStream.Write(lootOwnerID != LWOOBJID_EMPTY);
		if (lootOwnerID != LWOOBJID_EMPTY) {
			bitStream.Write(lootOwnerID);
		}

		SEND_PACKET_BROADCAST;
	}

	inline void SendDie(Entity* entity, const LWOOBJID& killerID, const LWOOBJID& lootOwnerID, bool bDieAccepted, eKillType killType, std::u16string deathType, float directionRelative_AngleY, float directionRelative_AngleXZ, float directionRelative_Force, bool bClientDeath, bool bSpawnLoot, float coinSpawnTime) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());

		bitStream.Write(MessageType::Game::DIE);

		bitStream.Write(bClientDeath);
		bitStream.Write(bSpawnLoot);

		//bitStream.Write(coinSpawnTime != -1.0f);
		//if (coinSpawnTime != -1.0f) bitStream.Write(coinSpawnTime);

		uint32_t deathTypeLength = deathType.size();
		bitStream.Write(deathTypeLength);
		for (uint32_t k = 0; k < deathTypeLength; k++) {
			bitStream.Write<uint16_t>(deathType[k]);
		}

		bitStream.Write(directionRelative_AngleXZ);
		bitStream.Write(directionRelative_AngleY);
		bitStream.Write(directionRelative_Force);

		bitStream.Write(killType != eKillType::VIOLENT);
		if (killType != eKillType::VIOLENT) bitStream.Write(killType);

		bitStream.Write(killerID);

		bitStream.Write(lootOwnerID != LWOOBJID_EMPTY);
		if (lootOwnerID != LWOOBJID_EMPTY) {
			bitStream.Write(lootOwnerID);
		}

		SEND_PACKET_BROADCAST;
	}

	inline void SendResurrect(Entity* entity) {
		// Restore the players health after the animation for respawning has finished.
		// This is when the health appered back in live, not immediately upon requesting respawn
		// Add a half second in case someone decides to cheat and move during the death animation
		// and just make sure the client has time to be ready.
		constexpr float respawnTime = 3.66700005531311f + 0.5f;
		entity->AddCallbackTimer(respawnTime, [=]() {
			GameMessages::PlayerResurrectionFinished msg;
			entity->NotifyPlayerResurrectionFinished(msg);
			auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

			if (destroyableComponent != nullptr && entity->GetLOT() == 1) {
				destroyableComponent->SetIsDead(false);
				auto* levelComponent = entity->GetComponent<LevelProgressionComponent>();
				if (levelComponent) {
					int32_t healthToRestore = levelComponent->GetLevel() >= 45 ? 8 : 4;
					if (healthToRestore > destroyableComponent->GetMaxHealth()) healthToRestore = destroyableComponent->GetMaxHealth();
					destroyableComponent->SetHealth(healthToRestore);

					int32_t imaginationToRestore = levelComponent->GetLevel() >= 45 ? 20 : 6;
					if (imaginationToRestore > destroyableComponent->GetMaxImagination()) imaginationToRestore = destroyableComponent->GetMaxImagination();
					destroyableComponent->SetImagination(imaginationToRestore);
				}
			}
			});

		CBITSTREAM;
		CMSGHEADER;

		bool bRezImmediately = false;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::RESURRECT);
		bitStream.Write(bRezImmediately);

		SEND_PACKET_BROADCAST;
	}

	inline void SendSetStunned(LWOOBJID objectId, eStateChangeType stateChangeType, const SystemAddress& sysAddr,
		LWOOBJID originator, bool bCantAttack, bool bCantEquip,
		bool bCantInteract, bool bCantJump, bool bCantMove, bool bCantTurn,
		bool bCantUseItem, bool bDontTerminateInteract, bool bIgnoreImmunity,
		bool bCantAttackOutChangeWasApplied, bool bCantEquipOutChangeWasApplied,
		bool bCantInteractOutChangeWasApplied, bool bCantJumpOutChangeWasApplied,
		bool bCantMoveOutChangeWasApplied, bool bCantTurnOutChangeWasApplied,
		bool bCantUseItemOutChangeWasApplied) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SET_STUNNED);

		bitStream.Write(originator != LWOOBJID_EMPTY);
		if (originator != LWOOBJID_EMPTY) bitStream.Write(originator);

		bitStream.Write(stateChangeType);

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

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendSetStunImmunity(LWOOBJID target, eStateChangeType state, const SystemAddress& sysAddr,
		LWOOBJID originator,
		bool bImmuneToStunAttack,
		bool bImmuneToStunEquip,
		bool bImmuneToStunInteract,
		bool bImmuneToStunJump,
		bool bImmuneToStunMove,
		bool bImmuneToStunTurn,
		bool bImmuneToStunUseItem) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(target);
		bitStream.Write(MessageType::Game::SET_STUN_IMMUNITY);

		bitStream.Write(originator != LWOOBJID_EMPTY);
		if (originator != LWOOBJID_EMPTY) bitStream.Write(originator);

		bitStream.Write(state);

		bitStream.Write(bImmuneToStunAttack);
		bitStream.Write(bImmuneToStunEquip);
		bitStream.Write(bImmuneToStunInteract);
		bitStream.Write(bImmuneToStunJump);
		bitStream.Write(bImmuneToStunMove);
		bitStream.Write(bImmuneToStunTurn);
		bitStream.Write(bImmuneToStunUseItem);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendSetStatusImmunity(LWOOBJID objectId, eStateChangeType state, const SystemAddress& sysAddr,
		bool bImmuneToBasicAttack,
		bool bImmuneToDamageOverTime,
		bool bImmuneToKnockback,
		bool bImmuneToInterrupt,
		bool bImmuneToSpeed,
		bool bImmuneToImaginationGain,
		bool bImmuneToImaginationLoss,
		bool bImmuneToQuickbuildInterrupt,
		bool bImmuneToPullToPoint) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SET_STATUS_IMMUNITY);

		bitStream.Write(state);

		bitStream.Write(bImmuneToBasicAttack);
		bitStream.Write(bImmuneToDamageOverTime);
		bitStream.Write(bImmuneToKnockback);
		bitStream.Write(bImmuneToInterrupt);
		bitStream.Write(bImmuneToSpeed);
		bitStream.Write(bImmuneToImaginationGain);
		bitStream.Write(bImmuneToImaginationLoss);
		bitStream.Write(bImmuneToQuickbuildInterrupt);
		bitStream.Write(bImmuneToPullToPoint);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendAddRunSpeedModifier(LWOOBJID objectId, LWOOBJID caster, uint32_t modifier, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::ADD_RUN_SPEED_MODIFIER);

		bitStream.Write(caster != LWOOBJID_EMPTY);
		if (caster != LWOOBJID_EMPTY) bitStream.Write(caster);

		bitStream.Write(modifier != 500);
		if (modifier != 500) bitStream.Write(modifier);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendRemoveRunSpeedModifier(LWOOBJID objectId, uint32_t modifier, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::REMOVE_RUN_SPEED_MODIFIER);

		bitStream.Write(modifier != 500);
		if (modifier != 500) bitStream.Write(modifier);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendRemoveBuff(Entity* entity, bool fromUnEquip, bool removeImmunity, uint32_t buffId) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::REMOVE_BUFF);

		bitStream.Write(false); // bFromRemoveBehavior but setting this to true makes the GM not do anything on the client?
		bitStream.Write(fromUnEquip);
		bitStream.Write(removeImmunity);
		bitStream.Write(buffId);

		SEND_PACKET_BROADCAST;
	}

	inline void SendSetResurrectRestoreValues(Entity* targetEntity, int32_t armorRestore, int32_t healthRestore, int32_t imaginationRestore) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(targetEntity->GetObjectID());
		bitStream.Write(MessageType::Game::SET_RESURRECT_RESTORE_VALUES);

		bitStream.Write(armorRestore != -1);
		if (armorRestore != -1) bitStream.Write(armorRestore);

		bitStream.Write(healthRestore != -1);
		if (healthRestore != -1) bitStream.Write(healthRestore);

		bitStream.Write(imaginationRestore != -1);
		if (imaginationRestore != -1) bitStream.Write(imaginationRestore);

		SEND_PACKET_BROADCAST;
	}

	inline void SendAddBuff(LWOOBJID& objectID, const LWOOBJID& casterID, uint32_t buffID, uint32_t msDuration,
		bool addImmunity, bool cancelOnDamaged, bool cancelOnDeath, bool cancelOnLogout,
		bool cancelOnRemoveBuff, bool cancelOnUi, bool cancelOnUnequip, bool cancelOnZone, bool addedByTeammate, bool applyOnTeammates,
		const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::ADD_BUFF);

		bitStream.Write(addedByTeammate); // Added by teammate
		bitStream.Write(applyOnTeammates); // Apply on teammates
		bitStream.Write(cancelOnDamaged);
		bitStream.Write(cancelOnDeath);
		bitStream.Write(cancelOnLogout);

		bitStream.Write(false); // Cancel on move
		bitStream.Write(cancelOnRemoveBuff);
		bitStream.Write(cancelOnUi);
		bitStream.Write(cancelOnUnequip);
		bitStream.Write(cancelOnZone);

		bitStream.Write(false); // Ignore immunities
		bitStream.Write(addImmunity);
		bitStream.Write(false); // Use ref count

		bitStream.Write(casterID != LWOOBJID_EMPTY);
		if (casterID != LWOOBJID_EMPTY) bitStream.Write(casterID);

		bitStream.Write(buffID);

		bitStream.Write(msDuration != 0);
		if (msDuration != 0) bitStream.Write(msDuration);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendDeactivateBubbleBuffFromServer(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::DEACTIVATE_BUBBLE_BUFF_FROM_SERVER);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendSmash(Entity* entity, float force, float ghostOpacity, LWOOBJID killerID, bool ignoreObjectVisibility) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::SMASH);

		bitStream.Write(ignoreObjectVisibility);
		bitStream.Write(force);
		bitStream.Write(ghostOpacity);
		bitStream.Write(killerID);

		SEND_PACKET_BROADCAST;
	}

	inline void SendUnSmash(Entity* entity, LWOOBJID builderID, float duration) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::UN_SMASH);

		bitStream.Write(builderID != LWOOBJID_EMPTY);
		if (builderID != LWOOBJID_EMPTY) bitStream.Write(builderID);

		bitStream.Write(duration != 3.0f);
		if (duration != 3.0f) bitStream.Write(duration);

		SEND_PACKET_BROADCAST;
	}

	// GameMessages::HandleRequestDie (the three argument overload the switch called)
	struct LegacyRequestDie { bool bClientDeath{}; bool bSpawnLoot{}; std::u16string deathType{}; float directionRelativeAngleXZ{}; float directionRelativeAngleY{}; float directionRelativeForce{}; eKillType killType{}; LWOOBJID killerID{}; LWOOBJID lootOwnerID{}; };
	inline LegacyRequestDie ReadRequestDie(RakNet::BitStream& inStream) {
		bool bClientDeath;
		bool bSpawnLoot;
		std::u16string deathType;
		float directionRelativeAngleXZ;
		float directionRelativeAngleY;
		float directionRelativeForce;
		eKillType killType = eKillType::VIOLENT;
		LWOOBJID killerID;
		LWOOBJID lootOwnerID = LWOOBJID_EMPTY;

		bClientDeath = inStream.ReadBit();
		bSpawnLoot = inStream.ReadBit();

		uint32_t deathTypeLength = 0;
		inStream.Read(deathTypeLength);

		for (size_t i = 0; i < deathTypeLength; i++) {
			char16_t character;
			inStream.Read(character);

			deathType.push_back(character);
		}

		inStream.Read(directionRelativeAngleXZ);
		inStream.Read(directionRelativeAngleY);
		inStream.Read(directionRelativeForce);

		if (inStream.ReadBit()) {
			inStream.Read(killType);
		}

		inStream.Read(killerID);

		if (inStream.ReadBit()) {
			inStream.Read(lootOwnerID);
		}
		return { bClientDeath, bSpawnLoot, deathType, directionRelativeAngleXZ, directionRelativeAngleY, directionRelativeForce, killType, killerID, lootOwnerID };
	}

	// GameMessages::HandleResurrect
	inline bool ReadResurrect(RakNet::BitStream& inStream) {
		bool immediate = inStream.ReadBit();
		return immediate;
	}

	// GameMessages::HandleActivateBubbleBuff
	struct LegacyActivateBubbleBuff { bool read{}; bool specialAnimations{}; std::u16string type{}; };
	inline LegacyActivateBubbleBuff ReadActivateBubbleBuff(RakNet::BitStream& inStream) {
		bool specialAnimations;
		if (!inStream.Read(specialAnimations)) return {};

		std::u16string type = GeneralUtils::ReadWString(inStream);
		return { true, specialAnimations, type };
	}
}

#endif // COMBATMESSAGESLEGACY_H
