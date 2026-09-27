#ifndef REMAININGMESSAGESLEGACY_H
#define REMAININGMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the last hand written GameMessages::Send* functions (dGame/dGameMessages/GameMessages.cpp)
// and WorldMigration's SendLocalizedAnnouncement, replaced by the Movement, Zone, Player, Script and QuickBuild
// message files and the rest of ActivityMessages. Only the namespace changed. The Read* functions are the read
// sequences of the replaced GameMessages::Handle* functions, verbatim up to the point where the handler starts
// using what it read.

#include "LegacyPacketMacros.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "Entity.h"
#include "Game.h"
#include "GameMessages.h"
#include "GeneralUtils.h"
#include "LeaderboardManager.h"
#include "MovementMessages.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "ServiceType.h"
#include "eControlScheme.h"
#include "eCyclingMode.h"
#include "eLootSourceType.h"
#include "eMatchUpdate.h"
#include "eMovementPlatformState.h"
#include "eObjectWorldState.h"
#include "eQuickBuildFailReason.h"
#include "eQuickBuildState.h"
#include "eTerminateType.h"
#include "eGameMasterLevel.h"

#include <string>

namespace LegacyGameMessages {
	inline void SendFireEventClientSide(const LWOOBJID& objectID, const SystemAddress& sysAddr, std::u16string args, const LWOOBJID& object, int64_t param1, int param2, const LWOOBJID& sender) {
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

	inline void SendTeleport(const LWOOBJID& objectID, const NiPoint3& pos, const NiQuaternion& rot, const SystemAddress& sysAddr, bool bSetRotation) {
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

	inline void SendPlayerReady(Entity* entity, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::PLAYER_READY);
		SEND_PACKET;
	}

	inline void SendInvalidZoneTransferList(Entity* entity, const SystemAddress& sysAddr, const std::u16string& feedbackURL, const std::u16string& invalidMapTransferList, bool feedbackOnExit, bool feedbackOnInvalidTransfer) {
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

	inline void SendPlayerSetCameraCyclingMode(const LWOOBJID& objectID, const SystemAddress& sysAddr,
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

	inline void SendStartPathing(Entity* entity) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::START_PATHING);

		SEND_PACKET_BROADCAST;
	}

	inline void SendPlatformResync(Entity* entity, const SystemAddress& sysAddr, bool bStopAtDesiredWaypoint,
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

	inline void SendRestoreToPostLoadStats(Entity* entity, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::RESTORE_TO_POST_LOAD_STATS);
		SEND_PACKET;
	}

	inline void SendServerDoneLoadingAllObjects(Entity* entity, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::SERVER_DONE_LOADING_ALL_OBJECTS);
		SEND_PACKET;
	}

	inline void SendChatModeUpdate(const LWOOBJID& objectID, eGameMasterLevel level) {
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::UPDATE_CHAT_MODE);
		bitStream.Write(level);
		SEND_PACKET_BROADCAST;
	}

	inline void SendGMLevelBroadcast(const LWOOBJID& objectID, eGameMasterLevel level) {
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::SET_GM_LEVEL);
		bitStream.Write1();
		bitStream.Write(level);
		SEND_PACKET_BROADCAST;
	}

	inline void SendChangeObjectWorldState(const LWOOBJID& objectID, eObjectWorldState state, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::CHANGE_OBJECT_WORLD_STATE);
		bitStream.Write(state);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST
			SEND_PACKET;
	}

	inline void SendModifyLEGOScore(Entity* entity, const SystemAddress& sysAddr, int64_t score, eLootSourceType sourceType) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::MODIFY_LEGO_SCORE);
		bitStream.Write(score);

		bitStream.Write(sourceType != eLootSourceType::NONE);
		if (sourceType != eLootSourceType::NONE) bitStream.Write(sourceType);

		SEND_PACKET;
	}

	inline void SendSetCurrency(Entity* entity, int64_t currency, int lootType, const LWOOBJID& sourceID, const LOT& sourceLOT, int sourceTradeID, bool overrideCurrent, eLootSourceType sourceType) {
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

	inline void SendQuickBuildNotifyState(Entity* entity, eQuickBuildState prevState, eQuickBuildState state, const LWOOBJID& playerID) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::REBUILD_NOTIFY_STATE);

		bitStream.Write(prevState);
		bitStream.Write(state);
		bitStream.Write(playerID);

		SEND_PACKET_BROADCAST;
	}

	inline void SendEnableQuickBuild(Entity* entity, bool enable, bool fail, bool success, eQuickBuildFailReason failReason, float duration, const LWOOBJID& playerID) {
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

	inline void SendTerminateInteraction(const LWOOBJID& objectID, eTerminateType type, const LWOOBJID& terminator) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::TERMINATE_INTERACTION);

		bitStream.Write(terminator);
		bitStream.Write(type);

		SEND_PACKET_BROADCAST;
	}

	inline void SendSetJetPackMode(Entity* entity, bool use, bool bypassChecks, bool doHover, int effectID, float airspeed, float maxAirspeed, float verticalVelocity, int warningEffectID) {
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

	inline void SendSetNetworkScriptVar(Entity* entity, const SystemAddress& sysAddr, std::string data) {
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

	inline void SendSetPlayerControlScheme(Entity* entity, eControlScheme controlScheme) {
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

	inline void SendPlayerReachedRespawnCheckpoint(Entity* entity, const NiPoint3& position, const NiQuaternion& rotation) {
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

	inline void SendMatchResponse(Entity* entity, const SystemAddress& sysAddr, int response) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::MATCH_RESPONSE);
		bitStream.Write(response);

		SEND_PACKET;
	}

	inline void SendMatchUpdate(Entity* entity, const SystemAddress& sysAddr, std::string data, eMatchUpdate type) {
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

	inline void SendRequestActivitySummaryLeaderboardData(const LWOOBJID& objectID, const LWOOBJID& targetID,
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

	inline void SendSetShootingGalleryParams(LWOOBJID objectId, const SystemAddress& sysAddr,
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

	inline void SendNotifyClientShootingGalleryScore(LWOOBJID objectId, const SystemAddress& sysAddr,
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

	inline void SendActivitySummaryLeaderboardData(const LWOOBJID& objectID, const Leaderboard* leaderboard, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::SEND_ACTIVITY_SUMMARY_LEADERBOARD_DATA);

		leaderboard->Serialize(bitStream);
		SEND_PACKET;
	}

	inline void SendSetRailMovement(const LWOOBJID& objectID, bool pathGoForward, std::u16string pathName,
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

	inline void SendStartRailMovement(const LWOOBJID& objectID, std::u16string pathName, std::u16string startSound,
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

	inline void SendNotifyClientObject(const LWOOBJID& objectID, std::u16string name, int param1, int param2, const LWOOBJID& paramObj, std::string paramStr, const SystemAddress& sysAddr) {
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

	inline void SendNotifyClientZoneObject(const LWOOBJID& objectID, const std::u16string& name, int param1,
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

	inline void SendNotifyClientFailedPrecondition(LWOOBJID objectId, const SystemAddress& sysAddr,
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

	inline void SendSetName(LWOOBJID objectID, std::u16string name, const SystemAddress& sysAddr) {
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

	inline void SendLockNodeRotation(Entity* entity, std::string nodeName) {
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

	inline void SendOrientToAngle(LWOOBJID objectId, bool bRelativeToCurrent, float fAngle, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::ORIENT_TO_ANGLE);

		bitStream.Write(bRelativeToCurrent);
		bitStream.Write(fAngle);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendNotifyObject(LWOOBJID objectId, LWOOBJID objIDSender, std::u16string name, const SystemAddress& sysAddr, int param1, int param2) {
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

	inline void SendTeamPickupItem(LWOOBJID objectId, LWOOBJID lootID, LWOOBJID lootOwnerID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::TEAM_PICKUP_ITEM);

		bitStream.Write(lootID);
		bitStream.Write(lootOwnerID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendDisplayZoneSummary(LWOOBJID objectId, const SystemAddress& sysAddr, bool isPropertyMap, bool isZoneStart, LWOOBJID sender) {
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

	inline void SendSetMountInventoryID(Entity* entity, const LWOOBJID& objectID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::SET_MOUNT_INVENTORY_ID);
		bitStream.Write(objectID != LWOOBJID_EMPTY);
		if (objectID != LWOOBJID_EMPTY) bitStream.Write(objectID);

		SEND_PACKET_BROADCAST;
	}

	inline void SendUpdateReputation(const LWOOBJID objectId, const int64_t reputation, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::UPDATE_REPUTATION);

		bitStream.Write(reputation);

		SEND_PACKET;
	}

	inline void SendSetGravityScale(const LWOOBJID& target, const float effectScale, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(target);
		bitStream.Write(MessageType::Game::SET_GRAVITY_SCALE);

		bitStream.Write(effectScale);

		SEND_PACKET;
	}

	inline void SendForceCameraTargetCycle(Entity* entity, bool bForceCycling, eCameraTargetCyclingMode cyclingMode, LWOOBJID optionalTargetID) {
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

	inline void WriteWString(RakNet::BitStream& bitStream, const std::u16string& text) {
		bitStream.Write<uint32_t>(text.size());
		for (const auto character : text) bitStream.Write<uint16_t>(character);
	}

	// LocalizedAnnouncementServerToSingleClient: the client looks both strings up in its locale (falling back to
	// the text itself) and shows the announcement popup, like its own instance-lock warning did in live
	inline void SendLocalizedAnnouncement(Entity* player, const std::u16string& body, const std::u16string& title) {
		const auto& sysAddr = player->GetSystemAddress();
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(player->GetObjectID());
		bitStream.Write(MessageType::Game::LOCALIZED_ANNOUNCEMENT_SERVER_TO_SINGLE_CLIENT);
		bitStream.Write<uint32_t>(0); // body parameters (LDF), none
		bitStream.Write(false); // force open chat box
		bitStream.Write(true); // show the announcement
		bitStream.Write(true); // and put the text in chat
		WriteWString(bitStream, body);
		WriteWString(bitStream, title);
		bitStream.Write<uint32_t>(0); // title parameters (LDF), none
		SEND_PACKET;
	}
	// Read sequences of the replaced GameMessages::Handle* functions.
	struct LegacyLeaderboardRequest { int32_t gameID; Leaderboard::InfoType queryType; int32_t resultsEnd; int32_t resultsStart; LWOOBJID target; bool weekly; };
	inline LegacyLeaderboardRequest ReadRequestActivitySummaryLeaderboardData(RakNet::BitStream& inStream) {
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
		return { gameID, queryType, resultsEnd, resultsStart, target, weekly };
	}

	struct LegacyActivityStateChangeRequest { LWOOBJID objectID; int32_t value1; int32_t value2; std::u16string stringValue; };
	inline LegacyActivityStateChangeRequest ReadActivityStateChangeRequest(RakNet::BitStream& inStream) {
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
		return { objectID, value1, value2, stringValue };
	}

	struct LegacyShootingGalleryRotation { float angle; NiPoint3 facing; NiPoint3 muzzlePos; };
	inline LegacyShootingGalleryRotation ReadUpdateShootingGalleryRotation(RakNet::BitStream& inStream) {
		float angle = 0.0f;
		NiPoint3 facing = NiPoint3Constant::ZERO;
		NiPoint3 muzzlePos = NiPoint3Constant::ZERO;
		inStream.Read(angle);
		inStream.Read(facing);
		inStream.Read(muzzlePos);
		return { angle, facing, muzzlePos };
	}

	struct LegacyVerifyAck { bool bDifferent; std::string sBitStream; uint32_t uiHandle; };
	inline LegacyVerifyAck ReadVerifyAck(RakNet::BitStream& inStream) {
		bool bDifferent;
		std::string sBitStream;
		uint32_t uiHandle = 0;

		bDifferent = inStream.ReadBit();

		uint32_t sBitStreamLength = 0;
		inStream.Read(sBitStreamLength);
		if (sBitStreamLength > MAX_MESSAGE_LENGTH) return {};
		for (uint64_t k = 0; k < sBitStreamLength; k++) {
			uint8_t character;
			inStream.Read(character);
			sBitStream.push_back(character);
		}

		if (inStream.ReadBit()) {
			inStream.Read(uiHandle);
		}
		return { bDifferent, sBitStream, uiHandle };
	}

	inline LWOOBJID ReadDismountComplete(RakNet::BitStream& inStream) {
		// Get the objectID from the bitstream
		LWOOBJID objectId{};
		inStream.Read(objectId);
		return objectId;
	}

	inline LWOOBJID ReadAcknowledgePossession(RakNet::BitStream& inStream) {
		bool hasObjectId{};
		inStream.Read(hasObjectId);
		LWOOBJID objectId{};
		if (hasObjectId) {
			inStream.Read(objectId);
		}
		return objectId;
	}

	inline bool ReadToggleGhostReferenceOverride(RakNet::BitStream& inStream) {
		bool bOverride = false;

		inStream.Read(bOverride);
		return bOverride;
	}

	inline NiPoint3 ReadSetGhostReferencePosition(RakNet::BitStream& inStream) {
		NiPoint3 position;

		inStream.Read(position);
		return position;
	}

	struct LegacyParseChatMessage { int iClientState; std::u16string wsString; };
	inline LegacyParseChatMessage ReadParseChatMessage(RakNet::BitStream& inStream) {
		std::u16string wsString;
		int iClientState;
		inStream.Read(iClientState);

		uint32_t wsStringLength;
		inStream.Read(wsStringLength);

		if (wsStringLength > MAX_MESSAGE_LENGTH) {
			wsStringLength = MAX_MESSAGE_LENGTH;
		}

		for (uint32_t i = 0; i < wsStringLength; ++i) {
			uint16_t character;
			inStream.Read(character);
			wsString.push_back(character);
		}
		return { iClientState, wsString };
	}

	struct LegacyFireEventServerSide { std::u16string args; int param1; int param2; int param3; LWOOBJID senderID; };
	inline LegacyFireEventServerSide ReadFireEventServerSide(RakNet::BitStream& inStream) {
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
		if (argsLength > MAX_MESSAGE_LENGTH) return {};
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
		return { args, param1, param2, param3, senderID };
	}

	struct LegacyQuickBuildCancel { bool bEarlyRelease; LWOOBJID userID; };
	inline LegacyQuickBuildCancel ReadQuickBuildCancel(RakNet::BitStream& inStream) {
		bool bEarlyRelease;
		LWOOBJID userID;

		inStream.Read(bEarlyRelease);
		inStream.Read(userID);
		return { bEarlyRelease, userID };
	}

	inline unsigned int ReadPickupCurrency(RakNet::BitStream& inStream) {
		unsigned int currency;
		inStream.Read(currency);
		return currency;
	}

	struct LegacyMatchRequest { LWOOBJID activator; std::string playerChoices; int type; int value; };
	inline LegacyMatchRequest ReadMatchRequest(RakNet::BitStream& inStream) {
		LWOOBJID activator;
		uint32_t playerChoicesLen;
		std::string playerChoices;
		int type;
		int value;

		inStream.Read(activator);
		inStream.Read(playerChoicesLen);
		if (playerChoicesLen > MAX_MESSAGE_LENGTH) return {};
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
		return { activator, playerChoices, type, value };
	}

	struct LegacyBugReport { std::string body; std::string clientVersion; std::string otherPlayer; std::string selection; };
	inline LegacyBugReport ReadReportBug(RakNet::BitStream& inStream) {
		LegacyBugReport reportInfo;

		//Reading:
		uint32_t messageLength;
		inStream.Read(messageLength);

		if (messageLength > MAX_MESSAGE_LENGTH) return {};

		for (uint32_t i = 0; i < (messageLength); ++i) {
			uint16_t character;
			inStream.Read(character);
			reportInfo.body.push_back(static_cast<char>(character));
		}

		uint32_t clientVersionLength;
		inStream.Read(clientVersionLength);
		if (clientVersionLength > MAX_MESSAGE_LENGTH) return {};
		for (unsigned int k = 0; k < clientVersionLength; k++) {
			unsigned char character;
			inStream.Read(character);
			reportInfo.clientVersion.push_back(character);
		}

		uint32_t nOtherPlayerIDLength;
		inStream.Read(nOtherPlayerIDLength);
		if (nOtherPlayerIDLength > MAX_MESSAGE_LENGTH) return {};
		for (unsigned int k = 0; k < nOtherPlayerIDLength; k++) {
			unsigned char character;
			inStream.Read(character);
			reportInfo.otherPlayer.push_back(character);
		}

		uint32_t selectionLength;
		inStream.Read(selectionLength);
		if (selectionLength > MAX_MESSAGE_LENGTH) return {};
		for (unsigned int k = 0; k < selectionLength; k++) {
			unsigned char character;
			inStream.Read(character);
			reportInfo.selection.push_back(character);
		}
		return reportInfo;
	}

	inline bool ReadCancelRailMovement(RakNet::BitStream& inStream) {
		const auto immediate = inStream.ReadBit();
		return immediate;
	}

	struct LegacyRailArrived { std::u16string pathName; int32_t waypointNumber; };
	inline LegacyRailArrived ReadPlayerRailArrivedNotification(RakNet::BitStream& inStream) {
		uint32_t pathNameLength;
		inStream.Read(pathNameLength);
		if (pathNameLength > MAX_MESSAGE_LENGTH) return {};
		std::u16string pathName;
		for (auto k = 0; k < pathNameLength; k++) {
			uint16_t c;
			inStream.Read(c);
			pathName.push_back(c);
		}

		int32_t waypointNumber;
		inStream.Read(waypointNumber);
		return { pathName, waypointNumber };
	}

	struct LegacyZoneStatistic { bool set; std::u16string statisticsName; int32_t value; LWOMAPID zone; };
	inline LegacyZoneStatistic ReadModifyPlayerZoneStatistic(RakNet::BitStream& inStream) {
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
		return { set, statisticsName, value, zone };
	}

	struct LegacyPlayerStatistic { int32_t updateID; int64_t updateValue; };
	inline LegacyPlayerStatistic ReadUpdatePlayerStatistic(RakNet::BitStream& inStream) {
		int32_t updateID;
		inStream.Read<int32_t>(updateID);

		int64_t updateValue;
		if (inStream.ReadBit()) {
			inStream.Read<int64_t>(updateValue);
		} else {
			updateValue = 1;
		}
		return { updateID, updateValue };
	}

	inline LWOOBJID ReadZoneSummaryDismissed(RakNet::BitStream& inStream) {
		LWOOBJID player_id;
		inStream.Read<LWOOBJID>(player_id);
		return player_id;
	}
}

#endif // REMAININGMESSAGESLEGACY_H
