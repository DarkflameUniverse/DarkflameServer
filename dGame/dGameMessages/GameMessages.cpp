#include "GameMessages.h"
#include "EffectsMessages.h"
#include "InventoryMessages.h"
#include "DashboardNotify.h"
#include "PlayerReports.h"
#include "EconomyLedger.h"
#include "User.h"
#include "Entity.h"
#include "BitStreamUtils.h"
#include "BitStream.h"
#include "Game.h"
#include "SlashCommandHandler.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Logger.h"
#include "GeneralUtils.h"
#include "Character.h"
#include "EntityManager.h"
#include "Database.h"
#include "dServer.h"
#include "ObjectIDManager.h"
#include "CppScripts.h"
#include "UserManager.h"
#include "ZoneInstanceManager.h"
#include "ClientPackets.h"
#include "Item.h"
#include "ZCompression.h"
#include "dConfig.h"
#include "TeamManager.h"
#include "ChatPackets.h"
#include "MultiZoneEntranceComponent.h"
#include "eUnequippableActiveType.h"
#include "eMovementPlatformState.h"
#include "LeaderboardManager.h"
#include "Amf3.h"
#include "Loot.h"
#include "eRacingTaskParam.h"
#include "eMissionTaskType.h"
#include "eMissionState.h"
#include "eObjectBits.h"
#include "eTriggerEventType.h"
#include "eMatchUpdate.h"
#include "eCyclingMode.h"
#include "eCinematicEvent.h"
#include "eQuickBuildFailReason.h"
#include "eControlScheme.h"
#include "eStateChangeType.h"
#include "ServiceType.h"
#include "ePlayerFlag.h"

#include <sstream>
#include <future>
#include <chrono>
#include <ranges>
#include "RakString.h"

//CDB includes:
#include "CDClientManager.h"
#include "CDEmoteTable.h"

//Component includes:
#include "ControllablePhysicsComponent.h"
#include "CharacterComponent.h"
#include "MissionOfferComponent.h"
#include "MissionComponent.h"
#include "DestroyableComponent.h"
#include "ScriptComponent.h"
#include "QuickBuildComponent.h"
#include "VendorComponent.h"
#include "InventoryComponent.h"
#include "RocketLaunchpadControlComponent.h"
#include "PropertyEntranceComponent.h"
#include "MovingPlatformComponent.h"
#include "PetComponent.h"
#include "ModuleAssemblyComponent.h"
#include "HavokVehiclePhysicsComponent.h"
#include "RenderComponent.h"
#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "RacingControlComponent.h"
#include "RailActivatorComponent.h"
#include "LevelProgressionComponent.h"
#include "DonationVendorComponent.h"
#include "GhostComponent.h"
#include "AchievementVendorComponent.h"

// Message includes:
#include "dZoneManager.h"
#include "PropertyManagementComponent.h"
#include "PropertyVendorComponent.h"
#include "TradingManager.h"
#include "ControlBehaviors.h"
#include "AMFDeserialize.h"
#include "eBlueprintSaveResponseType.h"
#include "eAnimationFlags.h"
#include "AmfSerialize.h"
#include "eReplicaComponentType.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "ePetAbilityType.h"
#include "ActivityManager.h"
#include "PlayerManager.h"
#include "eVendorTransactionResult.h"
#include "eReponseMoveItemBetweenInventoryTypeCode.h"

#include "CDComponentsRegistryTable.h"
#include "CDObjectsTable.h"
#include "eItemType.h"
#include "Lxfml.h"
#include "Sd0.h"

void GameMessages::SendFireEventClientSide(const LWOOBJID& objectID, const SystemAddress& sysAddr, std::u16string args, const LWOOBJID& object, int64_t param1, int param2, const LWOOBJID& sender) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::FIRE_EVENT_CLIENT_SIDE);

	//bitStream.Write(args);
	uint32_t argSize = args.size();
	bitStream.Write(argSize);
	for (uint32_t k = 0; k < argSize; k++) {
		bitStream.Write<uint16_t>(args[k]);
	}
	bitStream.Write(object);
	bitStream.Write0();
	//bitStream.Write(param1);
	bitStream.Write0();
	//bitStream.Write(param2);
	bitStream.Write(sender);

	SEND_PACKET;
}

void GameMessages::SendTeleport(const LWOOBJID& objectID, const NiPoint3& pos, const NiQuaternion& rot, const SystemAddress& sysAddr, bool bSetRotation) {
	CBITSTREAM;
	CMSGHEADER;
	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::TELEPORT);

	bool bIgnoreY = (pos.y == 0.0f);
	bool bUseNavmesh = false;
	bool bSkipAllChecks = false;
	//float w = 1.0f;
	//float x = 0.0f;
	//float y = 0.0f;
	//float z = 0.0f;

	bitStream.Write(bIgnoreY);
	bitStream.Write(bSetRotation);
	bitStream.Write(bSkipAllChecks);
	bitStream.Write(pos.x);
	bitStream.Write(pos.y);
	bitStream.Write(pos.z);
	bitStream.Write(bUseNavmesh);

	bitStream.Write(rot.w != 1.0f);
	if (rot.w != 1.0f) bitStream.Write(rot.w);

	bitStream.Write(rot.x);
	bitStream.Write(rot.y);
	bitStream.Write(rot.z);

	SEND_PACKET;
}

void GameMessages::SendPlayerReady(Entity* entity, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;
	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::PLAYER_READY);
	SEND_PACKET;
}

void GameMessages::SendPlayerAllowedRespawn(LWOOBJID entityID, bool doNotPromptRespawn, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entityID);
	bitStream.Write(MessageType::Game::SET_PLAYER_ALLOWED_RESPAWN);
	bitStream.Write(doNotPromptRespawn);

	SEND_PACKET;
}

void GameMessages::SendInvalidZoneTransferList(Entity* entity, const SystemAddress& sysAddr, const std::u16string& feedbackURL, const std::u16string& invalidMapTransferList, bool feedbackOnExit, bool feedbackOnInvalidTransfer) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::INVALID_ZONE_TRANSFER_LIST);

	uint32_t CustomerFeedbackURLLength = feedbackURL.size();
	bitStream.Write(CustomerFeedbackURLLength);
	for (uint32_t k = 0; k < CustomerFeedbackURLLength; k++) {
		bitStream.Write<uint16_t>(feedbackURL[k]);
	}

	uint32_t InvalidMapTransferListLength = invalidMapTransferList.size();
	bitStream.Write(InvalidMapTransferListLength);
	for (uint32_t k = 0; k < InvalidMapTransferListLength; k++) {
		bitStream.Write<uint16_t>(invalidMapTransferList[k]);
	}

	bitStream.Write(feedbackOnExit);
	bitStream.Write(feedbackOnInvalidTransfer);

	SEND_PACKET;
}

void GameMessages::SendKnockback(const LWOOBJID& objectID, const LWOOBJID& caster, const LWOOBJID& originator, int knockBackTimeMS, const NiPoint3& vector) {
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

void GameMessages::SendPlayerSetCameraCyclingMode(const LWOOBJID& objectID, const SystemAddress& sysAddr,
	bool bAllowCyclingWhileDeadOnly, eCyclingMode cyclingMode) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::PLAYER_SET_CAMERA_CYCLING_MODE);

	bitStream.Write(bAllowCyclingWhileDeadOnly);

	bitStream.Write(cyclingMode != eCyclingMode::ALLOW_CYCLE_TEAMMATES);
	if (cyclingMode != eCyclingMode::ALLOW_CYCLE_TEAMMATES) {
		bitStream.Write(cyclingMode);
	}

	SEND_PACKET;
}

void GameMessages::SendStartPathing(Entity* entity) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::START_PATHING);

	SEND_PACKET_BROADCAST;
}

void GameMessages::SendPlatformResync(Entity* entity, const SystemAddress& sysAddr, bool bStopAtDesiredWaypoint,
	int iIndex, int iDesiredWaypointIndex, int nextIndex,
	eMovementPlatformState movementState, bool special) {
	CBITSTREAM;
	CMSGHEADER;

	const auto objID = entity->GetObjectID();
	const auto lot = entity->GetLOT();

	if (lot == 12341 || lot == 5027 || lot == 5028 || lot == 14335 || lot == 14447 || lot == 14449 || lot == 11306 || lot == 11308 || lot == 9483) {
		iDesiredWaypointIndex = (lot == 11306 || lot == 11308) ? 1 : 0;
		iIndex = lot == 9483 ? 1 : 0;
		nextIndex = lot == 9483 && !special ? 1 : 0;
		bStopAtDesiredWaypoint = true;
		movementState = lot == 9483 && !special ? eMovementPlatformState::Stopped : eMovementPlatformState::Stationary;
	}

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::PLATFORM_RESYNC);

	bool bReverse = false;
	int eCommand = 0;
	int eUnexpectedCommand = 0;
	float fIdleTimeElapsed = 0.0f;
	float fMoveTimeElapsed = 0.0f;
	float fPercentBetweenPoints = 0.0f;
	NiPoint3 ptUnexpectedLocation = NiPoint3Constant::ZERO;
	NiQuaternion qUnexpectedRotation = QuatUtils::IDENTITY;

	bitStream.Write(bReverse);
	bitStream.Write(bStopAtDesiredWaypoint);
	bitStream.Write(eCommand);
	bitStream.Write(static_cast<int32_t>(movementState));
	bitStream.Write(eUnexpectedCommand);
	bitStream.Write(fIdleTimeElapsed);
	bitStream.Write(fMoveTimeElapsed);
	bitStream.Write(fPercentBetweenPoints);
	bitStream.Write(iDesiredWaypointIndex);
	bitStream.Write(iIndex);
	bitStream.Write(nextIndex);
	bitStream.Write(ptUnexpectedLocation.x);
	bitStream.Write(ptUnexpectedLocation.y);
	bitStream.Write(ptUnexpectedLocation.z);

	bitStream.Write(qUnexpectedRotation != QuatUtils::IDENTITY);
	if (qUnexpectedRotation != QuatUtils::IDENTITY) {
		bitStream.Write(qUnexpectedRotation.x);
		bitStream.Write(qUnexpectedRotation.y);
		bitStream.Write(qUnexpectedRotation.z);
		bitStream.Write(qUnexpectedRotation.w);
	}

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::SendRestoreToPostLoadStats(Entity* entity, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;
	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::RESTORE_TO_POST_LOAD_STATS);
	SEND_PACKET;
}

void GameMessages::SendServerDoneLoadingAllObjects(Entity* entity, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;
	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::SERVER_DONE_LOADING_ALL_OBJECTS);
	SEND_PACKET;
}

void GameMessages::SendChatModeUpdate(const LWOOBJID& objectID, eGameMasterLevel level) {
	CBITSTREAM;
	CMSGHEADER;
	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::UPDATE_CHAT_MODE);
	bitStream.Write(level);
	SEND_PACKET_BROADCAST;
}

void GameMessages::SendGMLevelBroadcast(const LWOOBJID& objectID, eGameMasterLevel level) {
	CBITSTREAM;
	CMSGHEADER;
	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::SET_GM_LEVEL);
	bitStream.Write1();
	bitStream.Write(level);
	SEND_PACKET_BROADCAST;
}

void GameMessages::SendChangeObjectWorldState(const LWOOBJID& objectID, eObjectWorldState state, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::CHANGE_OBJECT_WORLD_STATE);
	bitStream.Write(state);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST
		SEND_PACKET;
}

void GameMessages::SendModifyLEGOScore(Entity* entity, const SystemAddress& sysAddr, int64_t score, eLootSourceType sourceType) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::MODIFY_LEGO_SCORE);
	bitStream.Write(score);

	bitStream.Write(sourceType != eLootSourceType::NONE);
	if (sourceType != eLootSourceType::NONE) bitStream.Write(sourceType);

	SEND_PACKET;
}

void GameMessages::SendSetCurrency(Entity* entity, int64_t currency, int lootType, const LWOOBJID& sourceID, const LOT& sourceLOT, int sourceTradeID, bool overrideCurrent, eLootSourceType sourceType) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::SET_CURRENCY);

	bitStream.Write(currency);

	bitStream.Write(lootType != LOOTTYPE_NONE);
	if (lootType != LOOTTYPE_NONE) bitStream.Write(lootType);

	bitStream.Write(NiPoint3Constant::ZERO);

	bitStream.Write(sourceLOT != LOT_NULL);
	if (sourceLOT != LOT_NULL) bitStream.Write(sourceLOT);

	bitStream.Write(sourceID != LWOOBJID_EMPTY);
	if (sourceID != LWOOBJID_EMPTY) bitStream.Write(sourceID);

	bitStream.Write(sourceTradeID != LWOOBJID_EMPTY);
	if (sourceTradeID != LWOOBJID_EMPTY) bitStream.Write(sourceTradeID);

	bitStream.Write(sourceType != eLootSourceType::NONE);
	if (sourceType != eLootSourceType::NONE) bitStream.Write(sourceType);

	SystemAddress sysAddr = entity->GetSystemAddress();
	SEND_PACKET;
}

void GameMessages::SendQuickBuildNotifyState(Entity* entity, eQuickBuildState prevState, eQuickBuildState state, const LWOOBJID& playerID) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::REBUILD_NOTIFY_STATE);

	bitStream.Write(prevState);
	bitStream.Write(state);
	bitStream.Write(playerID);

	SEND_PACKET_BROADCAST;
}

void GameMessages::SendEnableQuickBuild(Entity* entity, bool enable, bool fail, bool success, eQuickBuildFailReason failReason, float duration, const LWOOBJID& playerID) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::ENABLE_REBUILD);

	bitStream.Write(enable);
	bitStream.Write(fail);
	bitStream.Write(success);

	bitStream.Write(failReason != eQuickBuildFailReason::NOT_GIVEN);
	if (failReason != eQuickBuildFailReason::NOT_GIVEN) bitStream.Write(failReason);

	bitStream.Write(duration);
	bitStream.Write(playerID);

	SEND_PACKET_BROADCAST;
}

void GameMessages::SendTerminateInteraction(const LWOOBJID& objectID, eTerminateType type, const LWOOBJID& terminator) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::TERMINATE_INTERACTION);

	bitStream.Write(terminator);
	bitStream.Write(type);

	SEND_PACKET_BROADCAST;
}

void GameMessages::SendDieNoImplCode(Entity* entity, const LWOOBJID& killerID, const LWOOBJID& lootOwnerID, eKillType killType, std::u16string deathType, float directionRelative_AngleY, float directionRelative_AngleXZ, float directionRelative_Force, bool bClientDeath, bool bSpawnLoot) {
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

void GameMessages::SendDie(Entity* entity, const LWOOBJID& killerID, const LWOOBJID& lootOwnerID, bool bDieAccepted, eKillType killType, std::u16string deathType, float directionRelative_AngleY, float directionRelative_AngleXZ, float directionRelative_Force, bool bClientDeath, bool bSpawnLoot, float coinSpawnTime) {
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

void GameMessages::SendSetJetPackMode(Entity* entity, bool use, bool bypassChecks, bool doHover, int effectID, float airspeed, float maxAirspeed, float verticalVelocity, int warningEffectID) {
	/* historical jamesster jetpack values
	if (bIsJamessterPhysics) {
		fAirspeed = 75;
		fMaxAirspeed = 75;
		fVertVel = 15;
	}
	*/

	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::SET_JET_PACK_MODE);

	bitStream.Write(bypassChecks);
	bitStream.Write(doHover);
	bitStream.Write(use);

	bitStream.Write(effectID != -1);
	if (effectID != -1) bitStream.Write(effectID);

	bitStream.Write(airspeed != 10);
	if (airspeed != 10) bitStream.Write(airspeed);

	bitStream.Write(maxAirspeed != 15);
	if (maxAirspeed != 15) bitStream.Write(maxAirspeed);

	bitStream.Write(verticalVelocity != 1);
	if (verticalVelocity != 1) bitStream.Write(verticalVelocity);

	bitStream.Write(warningEffectID != -1);
	if (warningEffectID != -1) bitStream.Write(warningEffectID);

	SEND_PACKET_BROADCAST;
}

void GameMessages::SendResurrect(Entity* entity) {
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

void GameMessages::SendSetNetworkScriptVar(Entity* entity, const SystemAddress& sysAddr, std::string data) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::SCRIPT_NETWORK_VAR_UPDATE);

	// FIXME: this is a bad place to need to do a conversion because we have no clue whether data is utf8 or plain ascii
	// an this has performance implications
	const auto u16Data = GeneralUtils::ASCIIToUTF16(data);
	uint32_t dataSize = static_cast<uint32_t>(u16Data.size());

	bitStream.Write(dataSize);
	for (auto value : u16Data) {
		bitStream.Write<uint16_t>(value);
	}
	if (dataSize > 0) bitStream.Write<uint16_t>(0);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::SendSetPlayerControlScheme(Entity* entity, eControlScheme controlScheme) {
	CBITSTREAM;
	CMSGHEADER;

	bool bDelayCamSwitchIfInCinematic = true;
	bool bSwitchCam = true;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::SET_PLAYER_CONTROL_SCHEME);

	bitStream.Write(bDelayCamSwitchIfInCinematic);
	bitStream.Write(bSwitchCam);

	bitStream.Write(controlScheme != eControlScheme::SCHEME_A);
	if (controlScheme != eControlScheme::SCHEME_A) bitStream.Write(controlScheme);

	SystemAddress sysAddr = entity->GetSystemAddress();
	SEND_PACKET;
}

void GameMessages::SendPlayerReachedRespawnCheckpoint(Entity* entity, const NiPoint3& position, const NiQuaternion& rotation) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::PLAYER_REACHED_RESPAWN_CHECKPOINT);

	bitStream.Write(position.x);
	bitStream.Write(position.y);
	bitStream.Write(position.z);

	const bool bIsNotIdentity = rotation != QuatUtils::IDENTITY;
	bitStream.Write(bIsNotIdentity);

	if (bIsNotIdentity) {
		bitStream.Write(rotation.w);
		bitStream.Write(rotation.x);
		bitStream.Write(rotation.y);
		bitStream.Write(rotation.z);
	}

	SystemAddress sysAddr = entity->GetSystemAddress();
	SEND_PACKET;
}

void GameMessages::SendAddSkill(Entity* entity, TSkillID skillID, BehaviorSlot slotID) {
	int AICombatWeight = 0;
	bool bFromSkillSet = false;
	int castType = 0;
	float fTimeSecs = -1.0f;
	int iTimesCanCast = -1;
	bool temporary = true;

	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::ADD_SKILL);

	bitStream.Write(AICombatWeight != 0);
	if (AICombatWeight != 0) bitStream.Write(AICombatWeight);

	bitStream.Write(bFromSkillSet);

	bitStream.Write(castType != 0);
	if (castType != 0) bitStream.Write(castType);

	bitStream.Write(fTimeSecs != -1.0f);
	if (fTimeSecs != -1.0f) bitStream.Write(fTimeSecs);

	bitStream.Write(iTimesCanCast != -1);
	if (iTimesCanCast != -1) bitStream.Write(iTimesCanCast);

	bitStream.Write(skillID);

	bitStream.Write(slotID != BehaviorSlot::Invalid);
	if (slotID != BehaviorSlot::Invalid) bitStream.Write(slotID);

	bitStream.Write(temporary);

	SystemAddress sysAddr = entity->GetSystemAddress();
	SEND_PACKET;
}

void GameMessages::SendRemoveSkill(Entity* entity, TSkillID skillID) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::REMOVE_SKILL);
	bitStream.Write(false);
	bitStream.Write(skillID);

	SystemAddress sysAddr = entity->GetSystemAddress();
	SEND_PACKET;
}

void GameMessages::SendMatchResponse(Entity* entity, const SystemAddress& sysAddr, int response) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::MATCH_RESPONSE);
	bitStream.Write(response);

	SEND_PACKET;
}

void GameMessages::SendMatchUpdate(Entity* entity, const SystemAddress& sysAddr, std::string data, eMatchUpdate type) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::MATCH_UPDATE);
	bitStream.Write<uint32_t>(data.size());
	for (char character : data) {
		bitStream.Write<uint16_t>(character);
	}
	if (data.size() > 0) bitStream.Write<uint16_t>(0);
	bitStream.Write(type);

	SEND_PACKET;
}

void GameMessages::SendRequestActivitySummaryLeaderboardData(const LWOOBJID& objectID, const LWOOBJID& targetID,
	const SystemAddress& sysAddr, const int32_t& gameID,
	const int32_t& queryType, const int32_t& resultsEnd,
	const int32_t& resultsStart, bool weekly) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::REQUEST_ACTIVITY_SUMMARY_LEADERBOARD_DATA);

	bitStream.Write(gameID != 0);
	if (gameID != 0) {
		bitStream.Write<int32_t>(gameID);
	}

	bitStream.Write(queryType != 1);
	if (queryType != 1) {
		bitStream.Write<int32_t>(queryType);
	}

	bitStream.Write(resultsEnd != 10);
	if (resultsEnd != 10) {
		bitStream.Write<int32_t>(resultsEnd);
	}

	bitStream.Write(resultsStart != 0);
	if (resultsStart != 0) {
		bitStream.Write<int32_t>(resultsStart);
	}

	bitStream.Write<LWOOBJID>(targetID);
	bitStream.Write(weekly);

	SEND_PACKET;
}

void GameMessages::SendSetShootingGalleryParams(LWOOBJID objectId, const SystemAddress& sysAddr,
	float cameraFOV,
	float cooldown,
	float minDistance,
	NiPoint3 muzzlePosOffset,
	NiPoint3 playerPosOffset,
	float projectileVelocity,
	float timeLimit,
	bool bUseLeaderboards) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::SET_SHOOTING_GALLERY_PARAMS);
	/*
	bitStream.Write<float>(cameraFOV);
	bitStream.Write<float>(cooldown);
	bitStream.Write<float>(minDistance);
	bitStream.Write<NiPoint3>(muzzlePosOffset);
	bitStream.Write<NiPoint3>(playerPosOffset);
	bitStream.Write<float>(projectileVelocity);
	bitStream.Write<float>(timeLimit);
	bitStream.Write<bool>(bUseLeaderboards);
	*/
	// No clue about the order here
	bitStream.Write<NiPoint3>(playerPosOffset);
	bitStream.Write<float>(projectileVelocity);
	bitStream.Write<float>(cooldown);
	bitStream.Write<NiPoint3>(muzzlePosOffset);
	bitStream.Write<float>(minDistance);
	bitStream.Write<float>(cameraFOV);
	bitStream.Write<bool>(bUseLeaderboards);
	bitStream.Write<float>(timeLimit);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}


void GameMessages::SendNotifyClientShootingGalleryScore(LWOOBJID objectId, const SystemAddress& sysAddr,
	float addTime,
	int32_t score,
	LWOOBJID target,
	NiPoint3 targetPos) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::NOTIFY_CLIENT_SHOOTING_GALLERY_SCORE);
	bitStream.Write<float>(addTime);
	bitStream.Write<int32_t>(score);
	bitStream.Write<LWOOBJID>(target);
	bitStream.Write<NiPoint3>(targetPos);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}


void GameMessages::HandleUpdateShootingGalleryRotation(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	float angle = 0.0f;
	NiPoint3 facing = NiPoint3Constant::ZERO;
	NiPoint3 muzzlePos = NiPoint3Constant::ZERO;
	inStream.Read(angle);
	inStream.Read(facing);
	inStream.Read(muzzlePos);
}


void GameMessages::HandleActivitySummaryLeaderboardData(RakNet::BitStream& inStream, Entity* entity,
	const SystemAddress& sysAddr) {
	LOG("We got mail!");
}

void GameMessages::SendActivitySummaryLeaderboardData(const LWOOBJID& objectID, const Leaderboard* leaderboard, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::SEND_ACTIVITY_SUMMARY_LEADERBOARD_DATA);

	leaderboard->Serialize(bitStream);
	SEND_PACKET;
}

void GameMessages::HandleRequestActivitySummaryLeaderboardData(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	int32_t gameID = 0;
	if (inStream.ReadBit()) inStream.Read(gameID);

	Leaderboard::InfoType queryType = Leaderboard::InfoType::MyStanding;
	if (inStream.ReadBit()) inStream.Read<Leaderboard::InfoType>(queryType);

	int32_t resultsEnd = 10;
	if (inStream.ReadBit()) inStream.Read(resultsEnd);

	int32_t resultsStart = 0;
	if (inStream.ReadBit()) inStream.Read(resultsStart);

	LWOOBJID target{};
	inStream.Read(target);

	bool weekly = inStream.ReadBit();

	// The client won't accept more than 10 results even if we wanted it to
	LeaderboardManager::SendLeaderboard(gameID, queryType, weekly, entity->GetObjectID(), entity->GetObjectID(), 10);
}

void GameMessages::HandleActivityStateChangeRequest(RakNet::BitStream& inStream, Entity* entity) {
	LWOOBJID objectID;
	inStream.Read<LWOOBJID>(objectID);

	int32_t value1;
	inStream.Read<int32_t>(value1);

	int32_t value2;
	inStream.Read<int32_t>(value2);

	uint32_t stringValueLength;
	inStream.Read<uint32_t>(stringValueLength);

	std::u16string stringValue;
	for (uint32_t i = 0; i < stringValueLength; ++i) {
		uint16_t character;
		inStream.Read(character);
		stringValue.push_back(character);
	}

	auto* assosiate = Game::entityManager->GetEntity(objectID);

	LOG("%s [%i, %i] from %i to %i", GeneralUtils::UTF16ToWTF8(stringValue).c_str(), value1, value2, entity->GetLOT(), assosiate != nullptr ? assosiate->GetLOT() : 0);

	std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SHOOTING_GALLERY);
	for (Entity* scriptEntity : scriptedActs) {
		scriptEntity->OnActivityStateChangeRequest(objectID, value1, value2, stringValue);
	}

	entity->OnActivityStateChangeRequest(objectID, value1, value2, stringValue);
}

void GameMessages::SendSetRailMovement(const LWOOBJID& objectID, bool pathGoForward, std::u16string pathName,
	uint32_t pathStart, const SystemAddress& sysAddr, int32_t railActivatorComponentID,
	LWOOBJID railActivatorObjectID) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::SET_RAIL_MOVEMENT);

	bitStream.Write(pathGoForward);

	bitStream.Write<uint32_t>(pathName.size());
	for (auto character : pathName) {
		bitStream.Write<uint16_t>(character);
	}

	bitStream.Write<uint32_t>(pathStart);

	const auto componentIDIsDefault = railActivatorComponentID == -1;
	bitStream.Write(!componentIDIsDefault);
	if (!componentIDIsDefault)
		bitStream.Write<int32_t>(railActivatorComponentID);

	const auto activatorObjectIDIsDefault = railActivatorObjectID == LWOOBJID_EMPTY;
	bitStream.Write(!activatorObjectIDIsDefault);
	if (!activatorObjectIDIsDefault)
		bitStream.Write<LWOOBJID>(railActivatorObjectID);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::SendStartRailMovement(const LWOOBJID& objectID, std::u16string pathName, std::u16string startSound,
	std::u16string loopSound, std::u16string stopSound, const SystemAddress& sysAddr,
	uint32_t pathStart, bool goForward, bool damageImmune, bool noAggro, bool notifyActor,
	bool showNameBillboard, bool cameraLocked, bool collisionEnabled, bool useDB,
	int32_t railComponentID, LWOOBJID railActivatorObjectID) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::START_RAIL_MOVEMENT);

	bitStream.Write(damageImmune);
	bitStream.Write(noAggro);
	bitStream.Write(notifyActor);
	bitStream.Write(showNameBillboard);
	bitStream.Write(cameraLocked);
	bitStream.Write(collisionEnabled);

	bitStream.Write<uint32_t>(loopSound.size());
	for (auto character : loopSound) {
		bitStream.Write<uint16_t>(character);
	}

	bitStream.Write(goForward);

	bitStream.Write<uint32_t>(pathName.size());
	for (auto character : pathName) {
		bitStream.Write<uint16_t>(character);
	}

	const auto pathStartIsDefault = pathStart == 0;
	bitStream.Write(!pathStartIsDefault);
	if (!pathStartIsDefault) {
		bitStream.Write<uint32_t>(pathStart);
	}

	const auto railComponentIDIsDefault = railComponentID == -1;
	bitStream.Write(!railComponentIDIsDefault);
	if (!railComponentIDIsDefault) {
		bitStream.Write<int32_t>(railComponentID);
	}

	const auto railObjectIDIsDefault = railActivatorObjectID == LWOOBJID_EMPTY;
	bitStream.Write(!railObjectIDIsDefault);
	if (!railObjectIDIsDefault) {
		bitStream.Write<LWOOBJID>(railActivatorObjectID);
	}

	bitStream.Write<uint32_t>(startSound.size());
	for (auto character : startSound) {
		bitStream.Write<uint16_t>(character);
	}

	bitStream.Write<uint32_t>(stopSound.size());
	for (auto character : stopSound) {
		bitStream.Write<uint16_t>(character);
	}

	bitStream.Write(useDB);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::SendNotifyClientObject(const LWOOBJID& objectID, std::u16string name, int param1, int param2, const LWOOBJID& paramObj, std::string paramStr, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::NOTIFY_CLIENT_OBJECT);

	bitStream.Write<uint32_t>(name.size());
	for (auto character : name) {
		bitStream.Write<uint16_t>(character);
	}

	bitStream.Write(param1);

	bitStream.Write(param2);

	bitStream.Write(paramObj);

	bitStream.Write<uint32_t>(paramStr.size());
	for (auto character : paramStr) {
		bitStream.Write(character);
	}

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::SendNotifyClientZoneObject(const LWOOBJID& objectID, const std::u16string& name, int param1,
	int param2, const LWOOBJID& paramObj, const std::string& paramStr,
	const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::NOTIFY_CLIENT_ZONE_OBJECT);

	bitStream.Write<uint32_t>(name.size());
	for (const auto& character : name) {
		bitStream.Write<uint16_t>(character);
	}

	bitStream.Write(param1);
	bitStream.Write(param2);
	bitStream.Write(paramObj);

	bitStream.Write<uint32_t>(paramStr.size());
	for (const auto& character : paramStr) {
		bitStream.Write(character);
	}

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::SendNotifyClientFailedPrecondition(LWOOBJID objectId, const SystemAddress& sysAddr,
	const std::u16string& failedReason, int preconditionID) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::NOTIFY_CLIENT_FAILED_PRECONDITION);

	bitStream.Write<uint32_t>(failedReason.size());
	for (uint16_t character : failedReason) {
		bitStream.Write<uint16_t>(character);
	}

	bitStream.Write(preconditionID);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::SendSetName(LWOOBJID objectID, std::u16string name, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectID);
	bitStream.Write(MessageType::Game::SET_NAME);

	bitStream.Write<uint32_t>(name.size());

	for (size_t i = 0; i < name.size(); ++i)
		bitStream.Write(name[i]);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

// Property

void GameMessages::SendLockNodeRotation(Entity* entity, std::string nodeName) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::LOCK_NODE_ROTATION);

	bitStream.Write<uint32_t>(nodeName.size());
	for (char character : nodeName) {
		bitStream.Write(character);
	}

	SEND_PACKET_BROADCAST;
}

void GameMessages::SendSmash(Entity* entity, float force, float ghostOpacity, LWOOBJID killerID, bool ignoreObjectVisibility) {
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

void GameMessages::SendUnSmash(Entity* entity, LWOOBJID builderID, float duration) {
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

void GameMessages::SendSetStunned(LWOOBJID objectId, eStateChangeType stateChangeType, const SystemAddress& sysAddr,
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

void GameMessages::SendSetStunImmunity(LWOOBJID target, eStateChangeType state, const SystemAddress& sysAddr,
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

void GameMessages::SendSetStatusImmunity(LWOOBJID objectId, eStateChangeType state, const SystemAddress& sysAddr,
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

void GameMessages::SendOrientToAngle(LWOOBJID objectId, bool bRelativeToCurrent, float fAngle, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::ORIENT_TO_ANGLE);

	bitStream.Write(bRelativeToCurrent);
	bitStream.Write(fAngle);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}


void GameMessages::SendAddRunSpeedModifier(LWOOBJID objectId, LWOOBJID caster, uint32_t modifier, const SystemAddress& sysAddr) {
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

void GameMessages::SendRemoveRunSpeedModifier(LWOOBJID objectId, uint32_t modifier, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::REMOVE_RUN_SPEED_MODIFIER);

	bitStream.Write(modifier != 500);
	if (modifier != 500) bitStream.Write(modifier);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::SendNotifyObject(LWOOBJID objectId, LWOOBJID objIDSender, std::u16string name, const SystemAddress& sysAddr, int param1, int param2) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::NOTIFY_OBJECT);

	bitStream.Write(objIDSender);
	bitStream.Write<uint32_t>(name.size());
	for (const auto character : name) {
		bitStream.Write(character);
	}

	bitStream.Write(param1);
	bitStream.Write(param2);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::HandleVerifyAck(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	bool bDifferent;
	std::string sBitStream;
	uint32_t uiHandle = 0;

	bDifferent = inStream.ReadBit();

	uint32_t sBitStreamLength = 0;
	inStream.Read(sBitStreamLength);
	if (sBitStreamLength > MAX_MESSAGE_LENGTH) return;
	for (uint64_t k = 0; k < sBitStreamLength; k++) {
		uint8_t character;
		inStream.Read(character);
		sBitStream.push_back(character);
	}

	if (inStream.ReadBit()) {
		inStream.Read(uiHandle);
	}
}

void GameMessages::SendTeamPickupItem(LWOOBJID objectId, LWOOBJID lootID, LWOOBJID lootOwnerID, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::TEAM_PICKUP_ITEM);

	bitStream.Write(lootID);
	bitStream.Write(lootOwnerID);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

//Pets:

void GameMessages::SendRemoveBuff(Entity* entity, bool fromUnEquip, bool removeImmunity, uint32_t buffId) {
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

void GameMessages::SendDisplayZoneSummary(LWOOBJID objectId, const SystemAddress& sysAddr, bool isPropertyMap, bool isZoneStart, LWOOBJID sender) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::DISPLAY_ZONE_SUMMARY);

	bitStream.Write(isPropertyMap);
	bitStream.Write(isZoneStart);
	bitStream.Write(sender != LWOOBJID_EMPTY);
	if (sender != LWOOBJID_EMPTY) bitStream.Write(sender);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

//UI

// Mounts

void GameMessages::SendSetMountInventoryID(Entity* entity, const LWOOBJID& objectID, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;
	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::SET_MOUNT_INVENTORY_ID);
	bitStream.Write(objectID != LWOOBJID_EMPTY);
	if (objectID != LWOOBJID_EMPTY) bitStream.Write(objectID);

	SEND_PACKET_BROADCAST;
}

void GameMessages::UseSkillSet::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(bRemove);
	bitStream.Write(possessedId != LWOOBJID_EMPTY);
	if (possessedId != LWOOBJID_EMPTY) bitStream.Write(possessedId);
	bitStream.Write(setId != -1);
	if (setId != -1) bitStream.Write(setId);
}

void GameMessages::HandleDismountComplete(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	// Get the objectID from the bitstream
	LWOOBJID objectId{};
	inStream.Read(objectId);

	// If we aren't possessing somethings, the don't do anything
	if (objectId != LWOOBJID_EMPTY) {
		auto* possessorComponent = entity->GetComponent<PossessorComponent>();
		auto* mount = Game::entityManager->GetEntity(objectId);
		// make sure we have the things we need and they aren't null
		if (possessorComponent && mount) {
			if (!possessorComponent->GetIsDismounting()) return;
			possessorComponent->SetIsDismounting(false);
			possessorComponent->SetPossessable(LWOOBJID_EMPTY);
			possessorComponent->SetPossessableType(ePossessionType::NO_POSSESSION);

			// character related things
			auto* character = entity->GetComponent<CharacterComponent>();
			if (character) {
				// If we had an active item turn it off
				if (possessorComponent->GetMountItemID() != LWOOBJID_EMPTY) {
					GameMessages::MarkInventoryItemAsActive markActive;
					markActive.target = entity->GetObjectID();
					markActive.bActive = false;
					markActive.iType = eUnequippableActiveType::MOUNT;
					markActive.itemID = possessorComponent->GetMountItemID();
					markActive.Send(entity->GetSystemAddress());
				}
				possessorComponent->SetMountItemID(LWOOBJID_EMPTY);
			}

			// Set that the controllabel phsyics comp is teleporting
			auto* controllablePhysicsComponent = entity->GetComponent<ControllablePhysicsComponent>();
			if (controllablePhysicsComponent) controllablePhysicsComponent->SetIsTeleporting(true);

			// Call dismoint on the possessable comp to let it handle killing the possessable
			auto* possessableComponent = mount->GetComponent<PossessableComponent>();
			if (possessableComponent) possessableComponent->Dismount();

			// Update the entity that was possessing
			Game::entityManager->SerializeEntity(entity);
		}
	}
}


void GameMessages::HandleAcknowledgePossession(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	Game::entityManager->SerializeEntity(entity);
	bool hasObjectId{};
	inStream.Read(hasObjectId);
	if (hasObjectId) {
		LWOOBJID objectId{};
		inStream.Read(objectId);
		auto* mount = Game::entityManager->GetEntity(objectId);
		if (mount) Game::entityManager->SerializeEntity(mount);
	}
}

//Racing





void GameMessages::HandleRequestDie(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
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




void GameMessages::SendUpdateReputation(const LWOOBJID objectId, const int64_t reputation, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::UPDATE_REPUTATION);

	bitStream.Write(reputation);

	SEND_PACKET;
}

void GameMessages::SendSetResurrectRestoreValues(Entity* targetEntity, int32_t armorRestore, int32_t healthRestore, int32_t imaginationRestore) {
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








void GameMessages::SendAddBuff(LWOOBJID& objectID, const LWOOBJID& casterID, uint32_t buffID, uint32_t msDuration,
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


// NT

//-----------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------- Handlers ------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------

void GameMessages::HandleToggleGhostReferenceOverride(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	bool bOverride = false;

	inStream.Read(bOverride);

	auto* player = PlayerManager::GetPlayer(sysAddr);

	if (player != nullptr) {
		auto* ghostComponent = entity->GetComponent<GhostComponent>();
		if (ghostComponent) ghostComponent->SetGhostOverride(bOverride);

		Game::entityManager->UpdateGhosting(player);
	}
}


void GameMessages::HandleSetGhostReferencePosition(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	NiPoint3 position;

	inStream.Read(position);

	auto* player = PlayerManager::GetPlayer(sysAddr);

	if (player != nullptr) {
		auto* ghostComponent = entity->GetComponent<GhostComponent>();
		if (ghostComponent) ghostComponent->SetGhostOverridePoint(position);

		Game::entityManager->UpdateGhosting(player);
	}
}


void GameMessages::HandleParseChatMessage(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	std::u16string wsString;
	int iClientState;
	inStream.Read(iClientState);

	uint32_t wsStringLength;
	inStream.Read(wsStringLength);

	if (wsStringLength > MAX_MESSAGE_LENGTH) {
		LOG("Max message length reached, capping message.");
		wsStringLength = MAX_MESSAGE_LENGTH;
	}

	for (uint32_t i = 0; i < wsStringLength; ++i) {
		uint16_t character;
		inStream.Read(character);
		wsString.push_back(character);
	}

	if (!wsString.empty() && wsString[0] == L'/') {
		SlashCommandHandler::HandleChatCommand(wsString, entity, sysAddr);
	}
}

void GameMessages::HandleFireEventServerSide(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	uint32_t argsLength{};
	std::u16string args{};
	bool param1IsDefault{};
	int param1 = -1;
	bool param2IsDefault{};
	int param2 = -1;
	bool param3IsDefault{};
	int param3 = -1;
	LWOOBJID senderID{};

	inStream.Read(argsLength);
	if (argsLength > MAX_MESSAGE_LENGTH) return;
	for (uint32_t i = 0; i < argsLength; ++i) {
		uint16_t character;
		inStream.Read(character);
		args.push_back(character);
	}
	inStream.Read(param1IsDefault);
	if (param1IsDefault) inStream.Read(param1);
	inStream.Read(param2IsDefault);
	if (param2IsDefault) inStream.Read(param2);
	inStream.Read(param3IsDefault);
	if (param3IsDefault) inStream.Read(param3);
	inStream.Read(senderID);

	auto* sender = Game::entityManager->GetEntity(senderID);
	auto* player = PlayerManager::GetPlayer(sysAddr);

	if (!player) {
		return;
	}

	// This should probably get it's own "ServerEvents" system or something at some point
	if (args == u"ZonePlayer") {
		// Should probably check to make sure they're using a launcher at some point before someone makes a hack that lets you testmap

		LWOCLONEID cloneId = 0;
		LWOMAPID mapId = 0;

		auto* rocketPad = entity->GetComponent<RocketLaunchpadControlComponent>();

		if (rocketPad == nullptr) return;

		cloneId = rocketPad->GetSelectedCloneId(player->GetObjectID());

		if (param2) {
			mapId = rocketPad->GetDefaultZone();
		} else {
			mapId = param3;
		}

		if (mapId == 0) {
			mapId = rocketPad->GetSelectedMapId(player->GetObjectID());
		}

		if (mapId == 0) {
			mapId = Game::zoneManager->GetZoneID().GetMapID(); // Fallback to sending the player back to the same zone.
		}

		LOG("Player %llu has requested zone transfer to (%i, %i).", sender->GetObjectID(), static_cast<int>(mapId), static_cast<int>(cloneId));

		auto* character = player->GetCharacter();

		if (mapId <= 0) {
			return;
		}

		ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, mapId, cloneId, false, [=](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
			LOG("Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", character->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);

			if (character) {
				auto* characterComponent = player->GetComponent<CharacterComponent>();
				if (characterComponent) {
					characterComponent->AddVisitedLevel(LWOZONEID(zoneID, LWOINSTANCEID_INVALID, zoneClone));
				}

				character->SetZoneID(zoneID);
				character->SetZoneInstance(zoneInstance);
				character->SetZoneClone(zoneClone);
			}

			ClientPackets::TransferToWorld transfer;
			transfer.serverIP = LUString(serverIP);
			transfer.serverPort = serverPort;
			transfer.mythranShift = mythranShift;
			transfer.Send(sysAddr);
			return;
			});
	}

	entity->OnFireEventServerSide(sender, GeneralUtils::UTF16ToWTF8(args), param1, param2, param3);
}

void GameMessages::HandleRequestPlatformResync(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	if (entity->GetLOT() == 6267 || entity->GetLOT() == 16141) return;
	GameMessages::SendPlatformResync(entity, sysAddr);
}

void GameMessages::HandleQuickBuildCancel(RakNet::BitStream& inStream, Entity* entity) {
	bool bEarlyRelease;
	LWOOBJID userID;

	inStream.Read(bEarlyRelease);
	inStream.Read(userID);

	auto* quickBuildComponent = static_cast<QuickBuildComponent*>(entity->GetComponent(eReplicaComponentType::QUICK_BUILD));;
	if (!quickBuildComponent) return;

	quickBuildComponent->CancelQuickBuild(Game::entityManager->GetEntity(userID), eQuickBuildFailReason::CANCELED_EARLY);
}

void GameMessages::HandleNotifyServerLevelProcessingComplete(RakNet::BitStream& inStream, Entity* entity) {
	auto* levelComp = entity->GetComponent<LevelProgressionComponent>();
	if (!levelComp) return;
	auto* character = entity->GetComponent<CharacterComponent>();
	if (!character) return;

	//Update our character's level in memory:
	levelComp->SetLevel(levelComp->GetLevel() + 1);

	levelComp->HandleLevelUp();

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

	if (inventoryComponent != nullptr) {
		auto* inventory = inventoryComponent->GetInventory(ITEMS);

		if (inventory != nullptr && Game::config->GetValue("disable_extra_backpack") != "1") {
			inventory->SetSize(inventory->GetSize() + 2);
		}
	}

	//Play the level up effect:
	GameMessages::PlayFXEffect(entity->GetObjectID(), 7074, u"create", "7074").Send(UNASSIGNED_SYSTEM_ADDRESS);

	//Send a notification in chat:
	std::stringstream wss;
	wss << "level=1:";
	wss << levelComp->GetLevel();
	wss << "\n";
	wss << "name=0:";
	wss << character->GetName();

	// FIXME: only really need utf8 conversion for the name, so move that up?
	std::u16string attrs = GeneralUtils::UTF8ToUTF16(wss.str());
	std::u16string wsText = u"UI_LEVEL_PROGRESSION_LEVELUP_MESSAGE";

	GameMessages::BroadcastTextToChatbox chatboxText;
	chatboxText.target = entity->GetObjectID();
	chatboxText.attrs = attrs;
	chatboxText.wsText = wsText;
	chatboxText.Send(UNASSIGNED_SYSTEM_ADDRESS);
}

void GameMessages::HandlePickupCurrency(RakNet::BitStream& inStream, Entity* entity) {
	unsigned int currency;
	inStream.Read(currency);

	if (currency == 0) return;

	auto* ch = entity->GetCharacter();
	if (ch && entity->PickupCoins(currency)) {
		ch->SetCoins(ch->GetCoins() + currency, eLootSourceType::PICKUP);
	}
}

void GameMessages::HandleRequestDie(RakNet::BitStream& inStream, Entity* entity) {
	LWOOBJID killerID;
	LWOOBJID lootOwnerID;
	bool bDieAccepted = false;
	eKillType killType;
	std::u16string deathType;
	float directionRelative_AngleY;
	float directionRelative_AngleXZ;
	float directionRelative_Force;
	bool bClientDeath = false;
	bool bSpawnLoot = true;
	float coinSpawnTime = -1.0f;

	inStream.Read(bClientDeath);
	inStream.Read(bDieAccepted);
	inStream.Read(bSpawnLoot);

	bool coinSpawnTimeIsDefault{};
	inStream.Read(coinSpawnTimeIsDefault);
	if (coinSpawnTimeIsDefault != 0) inStream.Read(coinSpawnTime);

	/*uint32_t deathTypeLength = deathType.size();
	inStream.Read(deathTypeLength);
	for (uint32_t k = 0; k < deathTypeLength; k++) {
		inStream.Read<uint16_t>(deathType[k]);
	}*/

	inStream.Read(directionRelative_AngleXZ);
	inStream.Read(directionRelative_AngleY);
	inStream.Read(directionRelative_Force);

	bool killTypeIsDefault{};
	inStream.Read(killTypeIsDefault);
	if (killTypeIsDefault != 0) inStream.Read(killType);

	inStream.Read(lootOwnerID);
	inStream.Read(killerID);
}

void GameMessages::SendSetGravityScale(const LWOOBJID& target, const float effectScale, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(target);
	bitStream.Write(MessageType::Game::SET_GRAVITY_SCALE);

	bitStream.Write(effectScale);

	SEND_PACKET;
}

void GameMessages::HandleResurrect(RakNet::BitStream& inStream, Entity* entity) {
	bool immediate = inStream.ReadBit();

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

void GameMessages::HandleMatchRequest(RakNet::BitStream& inStream, Entity* entity) {
	LWOOBJID activator;
	uint32_t playerChoicesLen;
	std::string playerChoices;
	int type;
	int value;

	inStream.Read(activator);
	inStream.Read(playerChoicesLen);
	if (playerChoicesLen > MAX_MESSAGE_LENGTH) return;
	for (uint32_t i = 0; i < playerChoicesLen; ++i) {
		uint16_t character;
		inStream.Read(character);
		playerChoices.push_back(character);
	}
	if (playerChoicesLen > 0) {
		uint16_t nullTerm;
		inStream.Read(nullTerm);
	}
	inStream.Read(type);
	inStream.Read(value);

	std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPTED_ACTIVITY);
	if (type == 0) { // join
		if (value != 0) {
			for (Entity* scriptedAct : scriptedActs) {
				ScriptedActivityComponent* comp = static_cast<ScriptedActivityComponent*>(scriptedAct->GetComponent(eReplicaComponentType::SCRIPTED_ACTIVITY));
				if (!comp) continue;
				if (comp->GetActivityID() == value) {
					comp->PlayerJoin(entity);
				}
			}
		} else {

		}
	} else if (type == 1) { // ready/unready
		for (Entity* scriptedAct : scriptedActs) {
			ScriptedActivityComponent* comp = static_cast<ScriptedActivityComponent*>(scriptedAct->GetComponent(eReplicaComponentType::SCRIPTED_ACTIVITY));
			if (!comp) continue;
			if (comp->PlayerIsInQueue(entity)) {
				comp->PlayerReady(entity, value);
			}
		}
	}
}

void GameMessages::HandleReportBug(RakNet::BitStream& inStream, Entity* entity) {
	//Definitely not stolen from autogenerated code, no sir:
	IBugReports::Info reportInfo;

	//Reading:
	uint32_t messageLength;
	inStream.Read(messageLength);

	if (messageLength > MAX_MESSAGE_LENGTH) return;

	for (uint32_t i = 0; i < (messageLength); ++i) {
		uint16_t character;
		inStream.Read(character);
		reportInfo.body.push_back(static_cast<char>(character));
	}

	auto character = entity->GetCharacter();
	if (character) reportInfo.characterId = character->GetID();

	uint32_t clientVersionLength;
	inStream.Read(clientVersionLength);
	if (clientVersionLength > MAX_MESSAGE_LENGTH) return;
	for (unsigned int k = 0; k < clientVersionLength; k++) {
		unsigned char character;
		inStream.Read(character);
		reportInfo.clientVersion.push_back(character);
	}

	uint32_t nOtherPlayerIDLength;
	inStream.Read(nOtherPlayerIDLength);
	if (nOtherPlayerIDLength > MAX_MESSAGE_LENGTH) return;
	for (unsigned int k = 0; k < nOtherPlayerIDLength; k++) {
		unsigned char character;
		inStream.Read(character);
		reportInfo.otherPlayer.push_back(character);
	}

	uint32_t selectionLength;
	inStream.Read(selectionLength);
	if (selectionLength > MAX_MESSAGE_LENGTH) return;
	for (unsigned int k = 0; k < selectionLength; k++) {
		unsigned char character;
		inStream.Read(character);
		reportInfo.selection.push_back(character);
	}

	// Report Abuse about another player sends their ID here (and "0" otherwise); that's a player report, not a bug
	if (const auto reportedId = GeneralUtils::TryParse<LWOOBJID>(reportInfo.otherPlayer).value_or(LWOOBJID_EMPTY); reportedId != LWOOBJID_EMPTY) {
		PlayerReports::ReportPlayer(entity, reportedId, reportInfo.body);
		return;
	}

	Database::Get()->InsertNewBugReport(reportInfo);
	DashboardNotify::Changed("bug_reports");
}

void
GameMessages::HandleClientRailMovementReady(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	const auto possibleRails = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RAIL_ACTIVATOR);
	for (const auto* possibleRail : possibleRails) {
		const auto* rail = possibleRail->GetComponent<RailActivatorComponent>();
		if (rail != nullptr) {
			rail->OnRailMovementReady(entity);
		}
	}
}

void GameMessages::HandleCancelRailMovement(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr) {
	const auto immediate = inStream.ReadBit();

	const auto possibleRails = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RAIL_ACTIVATOR);
	for (const auto* possibleRail : possibleRails) {
		auto* rail = possibleRail->GetComponent<RailActivatorComponent>();
		if (rail != nullptr) {
			rail->OnCancelRailMovement(entity);
		}
	}
}

void GameMessages::HandlePlayerRailArrivedNotification(RakNet::BitStream& inStream, Entity* entity,
	const SystemAddress& sysAddr) {
	uint32_t pathNameLength;
	inStream.Read(pathNameLength);
	if (pathNameLength > MAX_MESSAGE_LENGTH) return;
	std::u16string pathName;
	for (auto k = 0; k < pathNameLength; k++) {
		uint16_t c;
		inStream.Read(c);
		pathName.push_back(c);
	}

	int32_t waypointNumber;
	inStream.Read(waypointNumber);

	const auto possibleRails = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RAIL_ACTIVATOR);
	for (auto* possibleRail : possibleRails) {
		if (possibleRail) possibleRail->GetScript()->OnPlayerRailArrived(possibleRail, entity, pathName, waypointNumber);
	}
}

void GameMessages::HandleModifyPlayerZoneStatistic(RakNet::BitStream& inStream, Entity* entity) {
	const auto set = inStream.ReadBit();
	const auto statisticsName = GeneralUtils::ReadWString(inStream);

	int32_t value;
	if (inStream.ReadBit()) {
		inStream.Read<int32_t>(value);
	} else {
		value = 0;
	}

	LWOMAPID zone;
	if (inStream.ReadBit()) {
		inStream.Read<LWOMAPID>(zone);
	} else {
		zone = LWOMAPID_INVALID;
	}

	// Notify the character component that something's changed
	auto* characterComponent = entity->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->HandleZoneStatisticsUpdate(zone, statisticsName, value);
	}
}

void GameMessages::HandleUpdatePlayerStatistic(RakNet::BitStream& inStream, Entity* entity) {
	int32_t updateID;
	inStream.Read<int32_t>(updateID);

	int64_t updateValue;
	if (inStream.ReadBit()) {
		inStream.Read<int64_t>(updateValue);
	} else {
		updateValue = 1;
	}

	auto* characterComponent = entity->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->UpdatePlayerStatistic(static_cast<StatisticID>(updateID), static_cast<uint64_t>(std::max(updateValue, static_cast<int64_t>(0))), true);
	}
}

void GameMessages::HandleDeactivateBubbleBuff(RakNet::BitStream& inStream, Entity* entity) {
	auto controllablePhysicsComponent = entity->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) controllablePhysicsComponent->DeactivateBubbleBuff();
	GameMessages::SendDeactivateBubbleBuffFromServer(entity->GetObjectID(), entity->GetSystemAddress());
}

void GameMessages::HandleActivateBubbleBuff(RakNet::BitStream& inStream, Entity* entity) {
	bool specialAnimations;
	if (!inStream.Read(specialAnimations)) return;

	std::u16string type = GeneralUtils::ReadWString(inStream);
	auto bubbleType = eBubbleType::DEFAULT;
	if (type == u"skunk") bubbleType = eBubbleType::SKUNK;
	else if (type == u"energy") bubbleType = eBubbleType::ENERGY;

	auto controllablePhysicsComponent = entity->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) controllablePhysicsComponent->ActivateBubbleBuff(bubbleType, specialAnimations);
}

void GameMessages::SendActivateBubbleBuffFromServer(LWOOBJID objectId, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::ACTIVATE_BUBBLE_BUFF_FROM_SERVER);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::SendDeactivateBubbleBuffFromServer(LWOOBJID objectId, const SystemAddress& sysAddr) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(objectId);
	bitStream.Write(MessageType::Game::DEACTIVATE_BUBBLE_BUFF_FROM_SERVER);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
	SEND_PACKET;
}

void GameMessages::HandleZoneSummaryDismissed(RakNet::BitStream& inStream, Entity* entity) {
	LWOOBJID player_id;
	inStream.Read<LWOOBJID>(player_id);
	auto target = Game::entityManager->GetEntity(player_id);
	entity->TriggerEvent(eTriggerEventType::ZONE_SUMMARY_DISMISSED, target);
};

void GameMessages::SendForceCameraTargetCycle(Entity* entity, bool bForceCycling, eCameraTargetCyclingMode cyclingMode, LWOOBJID optionalTargetID) {
	CBITSTREAM;
	CMSGHEADER;

	bitStream.Write(entity->GetObjectID());
	bitStream.Write(MessageType::Game::FORCE_CAMERA_TARGET_CYCLE);
	bitStream.Write(bForceCycling);
	bitStream.Write(cyclingMode != eCameraTargetCyclingMode::ALLOW_CYCLE_TEAMMATES);
	if (cyclingMode != eCameraTargetCyclingMode::ALLOW_CYCLE_TEAMMATES) bitStream.Write(cyclingMode);
	bitStream.Write(optionalTargetID);

	auto sysAddr = entity->GetSystemAddress();
	SEND_PACKET;
}


namespace GameMessages {
	bool GameMsg::Send() {
		return Game::entityManager->SendMessage(*this);
	}

	bool GameMsg::Send(const LWOOBJID _target) {
		target = _target;
		return Send();
	}

	void NetGameMsg::WritePacket(RakNet::BitStream& bitStream) const {
		CMSGHEADER;

		bitStream.Write(target); // Who this message will be sent to on the (a) client
		bitStream.Write(msgId); // the ID of this message

		Serialize(bitStream); // write the message data
	}

	void NetGameMsg::SendToClient(const SystemAddress& sysAddr) const {
		CBITSTREAM;
		WritePacket(bitStream);
		SEND_PACKET;
	}

	void NetGameMsg::Send(const SystemAddress& sysAddr) const {
		CBITSTREAM;
		WritePacket(bitStream);

		// Send to everyone if someone sent unassigned system address, or to one specific client.
		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) {
			SEND_PACKET_BROADCAST;
		} else {
			SEND_PACKET;
		}
	}

	void DisplayTooltip::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(doOrDie);
		bitStream.Write(noRepeat);
		bitStream.Write(noRevive);
		bitStream.Write(isPropertyTooltip);
		bitStream.Write(show);
		bitStream.Write(translate);
		bitStream.Write(time);
		bitStream.Write<int32_t>(id.size());
		bitStream.Write(id);

		std::string toWrite;
		for (const auto& item : localizeParams | std::views::values) {
			toWrite += item->GetString() + "\n";
		}
		if (!toWrite.empty()) toWrite.pop_back();
		bitStream.Write<int32_t>(toWrite.size());
		bitStream.Write(GeneralUtils::ASCIIToUTF16(toWrite));
		if (!toWrite.empty()) bitStream.Write<uint16_t>(0x00); // Null Terminator

		bitStream.Write<int32_t>(imageName.size());
		bitStream.Write(imageName);
		bitStream.Write<int32_t>(text.size());
		bitStream.Write(text);
	}

	void UseItemOnClient::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(itemLOT);
		bitStream.Write(itemToUse);
		bitStream.Write(itemType);
		bitStream.Write(playerId);
		bitStream.Write(targetPosition.x);
		bitStream.Write(targetPosition.y);
		bitStream.Write(targetPosition.z);
	}

	void SetModelToBuild::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(modelLot != -1);
		if (modelLot != -1) bitStream.Write(modelLot);
	}

	void SpawnModelBricks::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(amount != 0.0f);
		if (amount != 0.0f) bitStream.Write(amount);
		bitStream.Write(position != NiPoint3Constant::ZERO);
		if (position != NiPoint3Constant::ZERO) {
			bitStream.Write(position.x);
			bitStream.Write(position.y);
			bitStream.Write(position.z);
		}
	}

	bool ShootingGalleryFire::Deserialize(RakNet::BitStream& bitStream) {
		if (!bitStream.Read(target.x)) return false;
		if (!bitStream.Read(target.y)) return false;
		if (!bitStream.Read(target.z)) return false;
		if (!bitStream.Read(rotation.w)) return false;
		if (!bitStream.Read(rotation.x)) return false;
		if (!bitStream.Read(rotation.y)) return false;
		if (!bitStream.Read(rotation.z)) return false;
		return true;
	}

	void ShootingGalleryFire::Handle(Entity& entity, const SystemAddress& sysAddr) {
		entity.OnShootingGalleryFire(*this);
	}

	bool RequestServerObjectInfo::Deserialize(RakNet::BitStream& bitStream) {
		if (!bitStream.Read(bVerbose)) return false;
		if (!bitStream.Read(clientId)) return false;
		if (!bitStream.Read(targetForReport)) return false;
		return true;
	}

	void RequestServerObjectInfo::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* handlingEntity = Game::entityManager->GetEntity(targetForReport);
		if (handlingEntity) {
			RequestServerObjectInfoEvent event(*this);
			handlingEntity->HandleMsg(event);
		} else LOG("Failed to find target %llu", targetForReport);
	}

	bool RequestUse::Deserialize(RakNet::BitStream& stream) {
		if (!stream.Read(bIsMultiInteractUse)) return false;
		if (!stream.Read(multiInteractID)) return false;
		if (!stream.Read(multiInteractType)) return false;
		if (!stream.Read(object)) return false;
		if (!stream.Read(secondary)) return false;
		return true;
	}

	void RequestUse::Handle(Entity& entity, const SystemAddress& sysAddr) {
		Entity* interactedObject = Game::entityManager->GetEntity(object);

		if (interactedObject == nullptr) {
			LOG("Object %llu tried to interact, but doesn't exist!", object);

			return;
		}

		if (interactedObject->GetLOT() == 9524) {
			entity.GetCharacter()->SetBuildMode(true);
		}

		if (bIsMultiInteractUse) {
			if (multiInteractType == 0) {
				auto* missionOfferComponent = static_cast<MissionOfferComponent*>(interactedObject->GetComponent(eReplicaComponentType::MISSION_OFFER));

				if (missionOfferComponent != nullptr) {
					missionOfferComponent->OfferMissions(&entity, multiInteractID);
				}
			} else {
				interactedObject->OnUse(&entity);
			}
		} else {
			interactedObject->OnUse(&entity);
		}

		RequestUseEvent event(*this);
		interactedObject->HandleMsg(event);

		//Perform use task if possible:
		auto missionComponent = entity.GetComponent<MissionComponent>();

		if (!missionComponent) return;

		missionComponent->Progress(eMissionTaskType::TALK_TO_NPC, interactedObject->GetLOT(), interactedObject->GetObjectID());
		missionComponent->Progress(eMissionTaskType::INTERACT, interactedObject->GetLOT(), interactedObject->GetObjectID());
	}

	void Smash::Serialize(RakNet::BitStream& stream) const {
		stream.Write(bIgnoreObjectVisibility);
		stream.Write(force);
		stream.Write(ghostCapacity);
		stream.Write(killerID);
	}

	void UnSmash::Serialize(RakNet::BitStream& stream) const {
		// Both fields are optional with a default, like the client's GameMessage::UnSmash::Serialize.
		BitStreamUtils::WriteOptional(stream, builderID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(stream, duration, 3.0f);
	}
	bool UnSmash::Deserialize(RakNet::BitStream& stream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(stream, builderID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(stream, duration, 3.0f));
		return true;
	}

	void PlayBehaviorSound::Serialize(RakNet::BitStream& stream) const {
		stream.Write(soundID != -1);
		if (soundID != -1) stream.Write(soundID);
	}

	void EmotePlayed::Serialize(RakNet::BitStream& stream) const {
		stream.Write(emoteID);
		stream.Write(targetID);
	}

	void DropClientLoot::Serialize(RakNet::BitStream& stream) const {
		stream.Write(bUsePosition);

		stream.Write(finalPosition != NiPoint3Constant::ZERO);
		if (finalPosition != NiPoint3Constant::ZERO) stream.Write(finalPosition);

		stream.Write(currency);
		stream.Write(item);
		stream.Write(lootID);
		stream.Write(ownerID);
		stream.Write(sourceID);

		stream.Write(spawnPos != NiPoint3Constant::ZERO);
		if (spawnPos != NiPoint3Constant::ZERO) stream.Write(spawnPos);
	}

	bool PickupItem::Deserialize(RakNet::BitStream& stream) {
		if (!stream.Read(lootID)) return false;
		if (!stream.Read(lootOwnerID)) return false;
		return true;
	}

	void PickupItem::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* team = TeamManager::Instance()->GetTeam(entity.GetObjectID());
		LOG("Has team %i picking up %llu:%llu", team != nullptr, lootID, lootOwnerID);
		if (team) {
			for (const auto memberId : team->members) {
				PickupItemEvent event(*this);
				event.Send(memberId);
				TeamPickupItem teamPickupMsg{};
				teamPickupMsg.target = lootID;
				teamPickupMsg.lootID = lootID;
				teamPickupMsg.lootOwnerID = lootOwnerID;
				const auto* const memberEntity = Game::entityManager->GetEntity(memberId);
				if (memberEntity) teamPickupMsg.Send(memberEntity->GetSystemAddress());
			}
		} else {
			entity.PickupItem(lootID);
		}
	}

	void TeamPickupItem::Serialize(RakNet::BitStream& stream) const {
		stream.Write(lootID);
		stream.Write(lootOwnerID);
	}

	void ToggleGMInvis::Serialize(RakNet::BitStream& stream) const {
		stream.Write(bStateOut);
	}
}
