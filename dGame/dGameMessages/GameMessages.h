#ifndef GAMEMESSAGES_H
#define GAMEMESSAGES_H

#include "dCommonVars.h"
#include <map>
#include <type_traits>
#include <string>
#include <vector>
#include "eMovementPlatformState.h"
#include "NiPoint3.h"
#include "eEndBehavior.h"
#include "eCyclingMode.h"
#include "eLootSourceType.h"
#include "Brick.h"
#include "MessageType/Game.h"
#include "eGameMasterLevel.h"
#include "LDFFormat.h"

class AMFBaseValue;
class AMFArrayValue;
class Entity;
class Item;
class User;
class Leaderboard;
class TradeItem;
class LDFBaseData;

enum class eAnimationFlags : uint32_t;

enum class eUnequippableActiveType;
enum eInventoryType : uint32_t;
enum class eGameMasterLevel : uint8_t;
enum class eMatchUpdate : int32_t;
enum class eKillType : uint32_t;
enum class eObjectWorldState : uint32_t;
enum class eTerminateType : uint32_t;
enum class eControlScheme : uint32_t;
enum class eStateChangeType : uint32_t;
enum class ePetAbilityType : uint32_t;
enum class ePetTamingNotifyType : uint32_t;
enum class eUseItemResponse : uint32_t;
enum class eQuickBuildFailReason : uint32_t;
enum class eQuickBuildState : uint32_t;
enum class BehaviorSlot : int32_t;
enum class eVendorTransactionResult : uint32_t;
enum class eReponseMoveItemBetweenInventoryTypeCode : int32_t;
enum class eMissionState : int;
enum class AiState : uint32_t;

enum class eCameraTargetCyclingMode : int32_t {
	ALLOW_CYCLE_TEAMMATES,
	DISALLOW_CYCLING
};

namespace GameMessages {
	/**
	 * A server-internal event, delivered only to the handlers entities/components/scripts registered with
	 * RegisterMsg. It never goes on the wire: it has no Serialize and no way to send it to a client.
	 */
	struct GameMsg {
		GameMsg(MessageType::Game gmId) : msgId{ gmId } {}
		virtual ~GameMsg() = default;

		// Delivers the message to the handlers registered on the target entity.
		bool Send();
		bool Send(const LWOOBJID _target);

		MessageType::Game msgId;
		LWOOBJID target{ LWOOBJID_EMPTY };
	};

	/**
	 * A game message that goes on the wire, to (Serialize) or from (Deserialize + Handle) a client.
	 * It is not a GameMsg, so it cannot be delivered to local handlers by mistake. When local handlers need to
	 * observe a network message, wrap it in a NetGameMsgEvent.
	 */
	struct NetGameMsg {
		NetGameMsg(MessageType::Game gmId, eGameMasterLevel lvl) : msgId{ gmId }, requiredGmLevel{ lvl } {}
		NetGameMsg(MessageType::Game gmId) : NetGameMsg(gmId, eGameMasterLevel::CIVILIAN) {}
		virtual ~NetGameMsg() = default;

		// Sends the message to the specified client or
		// all clients if UNASSIGNED_SYSTEM_ADDRESS is specified
		void Send(const SystemAddress& sysAddr) const;

		// Sends the message to the specified client only and never broadcasts. Given UNASSIGNED_SYSTEM_ADDRESS
		// nothing is delivered (RakNet rejects a non-broadcast send without an address).
		void SendToClient(const SystemAddress& sysAddr) const;

		// Writes the complete client packet (CLIENT/GAME_MSG header, target, msgId, then Serialize()) into bitStream.
		// This is exactly what Send(sysAddr) puts on the wire; tests use it to compare bytes without a server.
		void WritePacket(RakNet::BitStream& bitStream) const;

		virtual void Serialize(RakNet::BitStream& bitStream) const {}
		virtual bool Deserialize(RakNet::BitStream& bitStream) { return true; }

		// Called for messages received from a client, after a successful Deserialize.
		virtual void Handle(Entity& entity, const SystemAddress& sysAddr) {};

		MessageType::Game msgId;
		LWOOBJID target{ LWOOBJID_EMPTY };

		// Minimum GM level the sending client needs for Handle to be called.
		eGameMasterLevel requiredGmLevel;
	};

	/**
	 * A server-internal event carrying a network message, so entity/component handlers can react to a message
	 * received from (or about to be sent to) a client. Uses the network message's msgId for local dispatch.
	 */
	template<typename Msg>
	struct NetGameMsgEvent : public GameMsg {
		static_assert(std::is_base_of_v<NetGameMsg, Msg>, "NetGameMsgEvent carries a NetGameMsg");
		NetGameMsgEvent() : GameMsg(Msg{}.msgId) {}
		NetGameMsgEvent(const Msg& message) : GameMsg(message.msgId), msg{ message } { target = message.target; }

		Msg msg;
	};

	// Delivers a copy of a network message to the local handlers registered on msg.target (never to a client).
	// Returns whether a handler handled it. Handlers see (and may modify) only the copy.
	template<typename Msg>
	bool DeliverLocally(const Msg& msg) {
		NetGameMsgEvent<Msg> event(msg);
		return event.Send();
	}

	void SendFireEventClientSide(const LWOOBJID& objectID, const SystemAddress& sysAddr, std::u16string args, const LWOOBJID& object, int64_t param1, int param2, const LWOOBJID& sender);
	void SendTeleport(const LWOOBJID& objectID, const NiPoint3& pos, const NiQuaternion& rot, const SystemAddress& sysAddr, bool bSetRotation = false);
	void SendPlayerReady(Entity* entity, const SystemAddress& sysAddr);
	void SendPlayerAllowedRespawn(LWOOBJID entityID, bool doNotPromptRespawn, const SystemAddress& systemAddress);
	void SendInvalidZoneTransferList(Entity* entity, const SystemAddress& sysAddr, const std::u16string& feedbackURL, const std::u16string& invalidMapTransferList, bool feedbackOnExit, bool feedbackOnInvalidTransfer);
	void SendKnockback(const LWOOBJID& objectID, const LWOOBJID& caster, const LWOOBJID& originator, int knockBackTimeMS, const NiPoint3& vector);

	void SendPlayerSetCameraCyclingMode(const LWOOBJID& objectID, const SystemAddress& sysAddr, bool bAllowCyclingWhileDeadOnly = true, eCyclingMode cyclingMode = eCyclingMode::ALLOW_CYCLE_TEAMMATES);

	void SendStartPathing(Entity* entity);

	// special is for the FV tree platform, feature is complete if we just do that so meh
	void SendPlatformResync(Entity* entity, const SystemAddress& sysAddr, bool bStopAtDesiredWaypoint = false,
		int iIndex = 0, int iDesiredWaypointIndex = 1, int nextIndex = 1,
		eMovementPlatformState movementState = eMovementPlatformState::Moving, bool special = false);

	void SendRestoreToPostLoadStats(Entity* entity, const SystemAddress& sysAddr);
	void SendServerDoneLoadingAllObjects(Entity* entity, const SystemAddress& sysAddr);
	void SendGMLevelBroadcast(const LWOOBJID& objectID, eGameMasterLevel level);
	void SendChatModeUpdate(const LWOOBJID& objectID, eGameMasterLevel level);

	void SendChangeObjectWorldState(const LWOOBJID& objectID, eObjectWorldState state, const SystemAddress& sysAddr);

	void SendModifyLEGOScore(Entity* entity, const SystemAddress& sysAddr, int64_t score, eLootSourceType sourceType);

	void SendSetCurrency(Entity* entity, int64_t currency, int lootType, const LWOOBJID& sourceID, const LOT& sourceLOT, int sourceTradeID, bool overrideCurrent, eLootSourceType sourceType);

	void SendQuickBuildNotifyState(Entity* entity, eQuickBuildState prevState, eQuickBuildState state, const LWOOBJID& playerID);
	void SendEnableQuickBuild(Entity* entity, bool enable, bool fail, bool success, eQuickBuildFailReason failReason, float duration, const LWOOBJID& playerID);
	void AddActivityOwner(Entity* entity, LWOOBJID& ownerID);
	void SendTerminateInteraction(const LWOOBJID& objectID, eTerminateType type, const LWOOBJID& terminator);

	void SendDieNoImplCode(Entity* entity, const LWOOBJID& killerID, const LWOOBJID& lootOwnerID, eKillType killType, std::u16string deathType, float directionRelative_AngleY, float directionRelative_AngleXZ, float directionRelative_Force, bool bClientDeath, bool bSpawnLoot);
	void SendDie(Entity* entity, const LWOOBJID& killerID, const LWOOBJID& lootOwnerID, bool bDieAccepted, eKillType killType, std::u16string deathType, float directionRelative_AngleY, float directionRelative_AngleXZ, float directionRelative_Force, bool bClientDeath, bool bSpawnLoot, float coinSpawnTime);

	void SendSetGravityScale(const LWOOBJID& target, const float effectScale, const SystemAddress& sysAddr);

	void SendSetJetPackMode(Entity* entity, bool use, bool bypassChecks = false, bool doHover = false, int effectID = -1, float airspeed = 10, float maxAirspeed = 15, float verticalVelocity = 1, int warningEffectID = -1);
	void SendResurrect(Entity* entity);
	void SendSetNetworkScriptVar(Entity* entity, const SystemAddress& sysAddr, std::string data);

	void SendSetPlayerControlScheme(Entity* entity, eControlScheme controlScheme);
	void SendPlayerReachedRespawnCheckpoint(Entity* entity, const NiPoint3& position, const NiQuaternion& rotation);

	void SendAddSkill(Entity* entity, TSkillID skillID, BehaviorSlot slotID);
	void SendRemoveSkill(Entity* entity, TSkillID skillID);

	void SendMatchResponse(Entity* entity, const SystemAddress& sysAddr, int response);
	void SendMatchUpdate(Entity* entity, const SystemAddress& sysAddr, std::string data, eMatchUpdate type);

	void SendSetResurrectRestoreValues(Entity* targetEntity, int32_t armorRestore, int32_t healthRestore, int32_t imaginationRestore);

	/**
	 * Sends a message to an Entity to smash itself, but not delete or destroy itself from the world
	 *
	 * @param entity The Entity that will smash itself into bricks
	 * @param force The force the Entity will be smashed with
	 * @param ghostOpacity The ghosting opacity of the smashed Entity
	 * @param killerID The Entity that invoked the smash, if none exists, this should be LWOOBJID_EMPTY
	 * @param ignoreObjectVisibility Whether or not to ignore the objects visibility
	 */
	void SendSmash(Entity* entity, float force, float ghostOpacity, LWOOBJID killerID, bool ignoreObjectVisibility = false);

	/**
	 * Sends a message to an Entity to UnSmash itself (aka rebuild itself over a duration)
	 *
	 * @param entity The Entity that will UnSmash itself
	 * @param builderID The Entity that invoked the build (LWOOBJID_EMPTY if none exists or invoked the rebuild)
	 * @param duration The duration for the Entity to rebuild over.  3 seconds by default
	 */
	void SendUnSmash(Entity* entity, LWOOBJID builderID = LWOOBJID_EMPTY, float duration = 3.0f);

	// Rails stuff
	void SendSetRailMovement(const LWOOBJID& objectID, bool pathGoForward, std::u16string pathName, uint32_t pathStart,
		const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS,
		int32_t railActivatorComponentID = -1, LWOOBJID railActivatorObjectID = LWOOBJID_EMPTY);

	void SendStartRailMovement(const LWOOBJID& objectID, std::u16string pathName, std::u16string startSound,
		std::u16string loopSound, std::u16string stopSound, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS,
		uint32_t pathStart = 0, bool goForward = true, bool damageImmune = true, bool noAggro = true,
		bool notifyActor = false, bool showNameBillboard = true, bool cameraLocked = true,
		bool collisionEnabled = true, bool useDB = true, int32_t railComponentID = -1,
		LWOOBJID railActivatorObjectID = LWOOBJID_EMPTY);

	void HandleClientRailMovementReady(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleCancelRailMovement(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandlePlayerRailArrivedNotification(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendNotifyClientObject(const LWOOBJID& objectID, std::u16string name, int param1 = 0, int param2 = 0, const LWOOBJID& paramObj = LWOOBJID_EMPTY, std::string paramStr = "", const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);
	void SendNotifyClientZoneObject(const LWOOBJID& objectID, const std::u16string& name, int param1, int param2, const LWOOBJID& paramObj, const std::string& paramStr, const SystemAddress& sysAddr);

	void SendNotifyClientFailedPrecondition(LWOOBJID objectId, const SystemAddress& sysAddr, const std::u16string& failedReason, int preconditionID);

	void SendAddBuff(LWOOBJID& objectID, const LWOOBJID& casterID, uint32_t buffID, uint32_t msDuration,
		bool addImmunity = false, bool cancelOnDamaged = false, bool cancelOnDeath = true,
		bool cancelOnLogout = false, bool cancelOnRemoveBuff = true, bool cancelOnUi = false,
		bool cancelOnUnequip = false, bool cancelOnZone = false, bool addedByTeammate = false, bool applyOnTeammates = false, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);

	void SendSetName(LWOOBJID objectID, std::u16string name, const SystemAddress& sysAddr);

	void SendLockNodeRotation(Entity* entity, std::string nodeName);

	void SendSetStunned(LWOOBJID objectId, eStateChangeType stateChangeType, const SystemAddress& sysAddr,
		LWOOBJID originator = LWOOBJID_EMPTY, bool bCantAttack = false, bool bCantEquip = false,
		bool bCantInteract = false, bool bCantJump = false, bool bCantMove = false, bool bCantTurn = false,
		bool bCantUseItem = false, bool bDontTerminateInteract = false, bool bIgnoreImmunity = true,
		bool bCantAttackOutChangeWasApplied = false, bool bCantEquipOutChangeWasApplied = false,
		bool bCantInteractOutChangeWasApplied = false, bool bCantJumpOutChangeWasApplied = false,
		bool bCantMoveOutChangeWasApplied = false, bool bCantTurnOutChangeWasApplied = false,
		bool bCantUseItemOutChangeWasApplied = false);

	void SendSetStunImmunity(
		LWOOBJID target,
		eStateChangeType state,
		const SystemAddress& sysAddr,
		LWOOBJID originator = LWOOBJID_EMPTY,
		bool bImmuneToStunAttack = false,
		bool bImmuneToStunEquip = false,
		bool bImmuneToStunInteract = false,
		bool bImmuneToStunJump = false,
		bool bImmuneToStunMove = false,
		bool bImmuneToStunTurn = false,
		bool bImmuneToStunUseItem = false
	);

	void SendSetStatusImmunity(
		LWOOBJID objectId,
		eStateChangeType state,
		const SystemAddress& sysAddr,
		bool bImmuneToBasicAttack = false,
		bool bImmuneToDamageOverTime = false,
		bool bImmuneToKnockback = false,
		bool bImmuneToInterrupt = false,
		bool bImmuneToSpeed = false,
		bool bImmuneToImaginationGain = false,
		bool bImmuneToImaginationLoss = false,
		bool bImmuneToQuickbuildInterrupt = false,
		bool bImmuneToPullToPoint = false
	);

	void SendOrientToAngle(LWOOBJID objectId, bool bRelativeToCurrent, float fAngle, const SystemAddress& sysAddr);

	void SendAddRunSpeedModifier(LWOOBJID objectId, LWOOBJID caster, uint32_t modifier, const SystemAddress& sysAddr);

	void SendRemoveRunSpeedModifier(LWOOBJID objectId, uint32_t modifier, const SystemAddress& sysAddr);

	void SendNotifyObject(LWOOBJID objectId, LWOOBJID objIDSender, std::u16string name, const SystemAddress& sysAddr, int param1 = 0, int param2 = 0);

	void HandleVerifyAck(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendTeamPickupItem(LWOOBJID objectId, LWOOBJID lootID, LWOOBJID lootOwnerID, const SystemAddress& sysAddr);

	//Pets:

	void SendDisplayZoneSummary(LWOOBJID objectId, const SystemAddress& sysAddr, bool isPropertyMap = false, bool isZoneStart = false, LWOOBJID sender = LWOOBJID_EMPTY);

	// Mounts
	/**
	 * @brief Set the Inventory LWOOBJID of the mount
	 *
	 * @param entity The entity that is mounting
	 * @param sysAddr the system address to send game message responses to
	 * @param objectID LWOOBJID of the item in inventory that is being used
	 */
	void SendSetMountInventoryID(Entity* entity, const LWOOBJID& objectID, const SystemAddress& sysAddr);

	/**
	 * @brief Handle client dismounting mount
	 *
	 * @param inStream Raknet BitStream of incoming data
	 * @param entity The Entity that is dismounting
	 * @param sysAddr the system address to send game message responses to
	 */
	void HandleDismountComplete(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);

	/**
	 * @brief Handle acknowledging that the client possessed something
	 *
	 * @param inStream Raknet BitStream of incoming data
	 * @param entity The Entity that is possessing
	 * @param sysAddr the system address to send game message responses to
	 */
	void HandleAcknowledgePossession(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);

	//Racing:
	void HandleRequestDie(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);

	// SG:

	void SendSetShootingGalleryParams(LWOOBJID objectId, const SystemAddress& sysAddr,
		float cameraFOV,
		float cooldown,
		float minDistance,
		NiPoint3 muzzlePosOffset,
		NiPoint3 playerPosOffset,
		float projectileVelocity,
		float timeLimit,
		bool bUseLeaderboards
	);

	void SendNotifyClientShootingGalleryScore(LWOOBJID objectId, const SystemAddress& sysAddr,
		float addTime,
		int32_t score,
		LWOOBJID target,
		NiPoint3 targetPos
	);

	void HandleUpdateShootingGalleryRotation(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendUpdateReputation(const LWOOBJID objectId, const int64_t reputation, const SystemAddress& sysAddr);

	// Leaderboards
	void SendActivitySummaryLeaderboardData(const LWOOBJID& objectID, const Leaderboard* leaderboard,
		const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);
	void HandleActivitySummaryLeaderboardData(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void SendRequestActivitySummaryLeaderboardData(const LWOOBJID& objectID, const LWOOBJID& targetID,
		const SystemAddress& sysAddr, const int32_t& gameID = 0,
		const int32_t& queryType = 1, const int32_t& resultsEnd = 10,
		const int32_t& resultsStart = 0, bool weekly = false);
	void HandleRequestActivitySummaryLeaderboardData(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleActivityStateChangeRequest(RakNet::BitStream& inStream, Entity* entity);

	//NT:

	//Handlers:

	void HandleToggleGhostReferenceOverride(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleSetGhostReferencePosition(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleParseChatMessage(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleToggleGhostReffrenceOverride(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleSetGhostReffrenceOverride(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleFireEventServerSide(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleRequestPlatformResync(RakNet::BitStream& inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleQuickBuildCancel(RakNet::BitStream& inStream, Entity* entity);
	void HandleNotifyServerLevelProcessingComplete(RakNet::BitStream& inStream, Entity* entity);
	void HandlePickupCurrency(RakNet::BitStream& inStream, Entity* entity);
	void HandleRequestDie(RakNet::BitStream& inStream, Entity* entity);
	void HandlePickupItem(RakNet::BitStream& inStream, Entity* entity);
	void HandleResurrect(RakNet::BitStream& inStream, Entity* entity);
	void HandleModifyPlayerZoneStatistic(RakNet::BitStream& inStream, Entity* entity);
	void HandleUpdatePlayerStatistic(RakNet::BitStream& inStream, Entity* entity);

	void HandleMatchRequest(RakNet::BitStream& inStream, Entity* entity);

	void HandleReportBug(RakNet::BitStream& inStream, Entity* entity);

	void SendRemoveBuff(Entity* entity, bool fromUnEquip, bool removeImmunity, uint32_t buffId);

	// bubble
	void HandleDeactivateBubbleBuff(RakNet::BitStream& inStream, Entity* entity);

	void HandleActivateBubbleBuff(RakNet::BitStream& inStream, Entity* entity);

	void SendActivateBubbleBuffFromServer(LWOOBJID objectId, const SystemAddress& sysAddr);

	void SendDeactivateBubbleBuffFromServer(LWOOBJID objectId, const SystemAddress& sysAddr);

	void HandleZoneSummaryDismissed(RakNet::BitStream& inStream, Entity* entity);

	void SendForceCameraTargetCycle(Entity* entity, bool bForceCycling, eCameraTargetCyclingMode cyclingMode, LWOOBJID optionalTargetID);

	struct DisplayTooltip : public NetGameMsg {
		DisplayTooltip() : NetGameMsg(MessageType::Game::DISPLAY_TOOLTIP) {}
		bool doOrDie{};
		bool noRepeat{};
		bool noRevive{};
		bool isPropertyTooltip{};
		bool show{};
		bool translate{};
		int32_t time{};
		std::u16string id{};
		LwoNameValue localizeParams{};
		std::u16string imageName{};
		std::u16string text{};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct UseItemOnClient : public NetGameMsg {
		UseItemOnClient() : NetGameMsg(MessageType::Game::USE_ITEM_ON_CLIENT) {}
		LWOOBJID playerId{};
		LWOOBJID itemToUse{};
		uint32_t itemType{};
		LOT itemLOT{};
		NiPoint3 targetPosition{};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct ZoneLoadedInfo : public GameMsg {
		ZoneLoadedInfo() : GameMsg(MessageType::Game::ZONE_LOADED_INFO) {}
		int32_t maxPlayers{};
	};

	struct ConfigureRacingControl : public GameMsg {
		ConfigureRacingControl() : GameMsg(MessageType::Game::CONFIGURE_RACING_CONTROL) {}
		LwoNameValue racingSettings{};
	};

	struct SetModelToBuild : public NetGameMsg {
		SetModelToBuild() : NetGameMsg(MessageType::Game::SET_MODEL_TO_BUILD) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		LOT modelLot{ -1 };
	};

	struct SpawnModelBricks : public NetGameMsg {
		SpawnModelBricks() : NetGameMsg(MessageType::Game::SPAWN_MODEL_BRICKS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;

		float amount{ 0.0f };
		NiPoint3 position{ NiPoint3Constant::ZERO };
	};

	struct ActivityNotify : public GameMsg {
		ActivityNotify() : GameMsg(MessageType::Game::ACTIVITY_NOTIFY) {}

		LwoNameValue notification{};
	};

	struct ShootingGalleryFire : public NetGameMsg {
		ShootingGalleryFire() : NetGameMsg(MessageType::Game::SHOOTING_GALLERY_FIRE) {}
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		NiPoint3 target{};
		NiQuaternion rotation = QuatUtils::IDENTITY;
	};

	struct ChildLoaded : public GameMsg {
		ChildLoaded() : GameMsg(MessageType::Game::CHILD_LOADED) {}

		LOT templateID{};
		LWOOBJID childID{};
	};

	struct PlayerResurrectionFinished : public GameMsg {
		PlayerResurrectionFinished() : GameMsg(MessageType::Game::PLAYER_RESURRECTION_FINISHED) {}
	};

	struct RequestServerObjectInfo : public NetGameMsg {
		bool bVerbose{};
		LWOOBJID clientId{};
		LWOOBJID targetForReport{};

		RequestServerObjectInfo() : NetGameMsg(MessageType::Game::REQUEST_SERVER_OBJECT_INFO, eGameMasterLevel::DEVELOPER) {}
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};
	using RequestServerObjectInfoEvent = NetGameMsgEvent<RequestServerObjectInfo>;

	struct GetObjectReportInfo : public GameMsg {
		AMFArrayValue* info{};
		AMFArrayValue* subCategory{};
		bool bVerbose{};
		LWOOBJID clientID{};

		GetObjectReportInfo() : GameMsg(MessageType::Game::GET_OBJECT_REPORT_INFO) {}
	};

	struct RequestUse : public NetGameMsg {
		RequestUse() : NetGameMsg(MessageType::Game::REQUEST_USE) {}

		bool Deserialize(RakNet::BitStream& stream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID object{};

		bool secondary{ false };

		// Set to true if this coming from a multi-interaction UI on the client.
		bool bIsMultiInteractUse{};

		// Used only for multi-interaction
		unsigned int multiInteractID{};

		// Used only for multi-interaction, is of the enum type InteractionType
		int multiInteractType{};
	};
	using RequestUseEvent = NetGameMsgEvent<RequestUse>;

	struct Smash : public NetGameMsg {
		Smash() : NetGameMsg(MessageType::Game::SMASH) {}

		void Serialize(RakNet::BitStream& stream) const override;

		bool bIgnoreObjectVisibility{};
		bool force{};
		float ghostCapacity{};
		LWOOBJID killerID{};
	};

	struct UnSmash : public NetGameMsg {
		UnSmash() : NetGameMsg(MessageType::Game::UN_SMASH) {}

		void Serialize(RakNet::BitStream& stream) const override;
		bool Deserialize(RakNet::BitStream& stream) override;

		LWOOBJID builderID{ LWOOBJID_EMPTY };
		float duration{ 3.0f };
	};

	struct PlayBehaviorSound : public NetGameMsg {
		PlayBehaviorSound() : NetGameMsg(MessageType::Game::PLAY_BEHAVIOR_SOUND) {}

		void Serialize(RakNet::BitStream& stream) const override;

		int32_t soundID{ -1 };
	};

	struct ResetModelToDefaults : public GameMsg {
		ResetModelToDefaults() : GameMsg(MessageType::Game::RESET_MODEL_TO_DEFAULTS) {}

		bool bResetPos{ true };
		bool bResetRot{ true };
		bool bUnSmash{ true };
		bool bResetBehaviors{ true };
	};

	struct EmotePlayed : public NetGameMsg {
		EmotePlayed() : NetGameMsg(MessageType::Game::EMOTE_PLAYED), emoteID(0), targetID(0) {}

		void Serialize(RakNet::BitStream& stream) const override;

		int32_t emoteID;
		LWOOBJID targetID;
	};

	struct GetPosition : public GameMsg {
		GetPosition() : GameMsg(MessageType::Game::GET_POSITION) {}

		NiPoint3 pos{};
	};

	struct SetFaction : public GameMsg {
		SetFaction() : GameMsg(MessageType::Game::SET_FACTION) {}

		int32_t factionID{};

		bool bIgnoreChecks{ false };
	};

	struct DropClientLoot : public NetGameMsg {
		DropClientLoot() : NetGameMsg(MessageType::Game::DROP_CLIENT_LOOT) {}

		void Serialize(RakNet::BitStream& stream) const override;
		LWOOBJID sourceID{ LWOOBJID_EMPTY };
		LOT item{ LOT_NULL };
		int32_t currency{};
		NiPoint3 spawnPos{};
		NiPoint3 finalPosition{};
		int32_t count{};
		bool bUsePosition{};
		LWOOBJID lootID{ LWOOBJID_EMPTY };
		LWOOBJID ownerID{ LWOOBJID_EMPTY };
	};
	using DropClientLootEvent = NetGameMsgEvent<DropClientLoot>;

	struct GetMissionState : public GameMsg {
		GetMissionState() : GameMsg(MessageType::Game::GET_MISSION_STATE) {}

		int32_t missionID{};
		eMissionState missionState{};
		bool cooldownInfoRequested{};
		bool cooldownFinished{};
	};

	struct GetFlag : public GameMsg {
		GetFlag() : GameMsg(MessageType::Game::GET_FLAG) {}

		uint32_t flagID{};
		bool flag{};
	};

	struct GetFactionTokenType : public GameMsg {
		GetFactionTokenType() : GameMsg(MessageType::Game::GET_FACTION_TOKEN_TYPE) {}

		LOT tokenType{ LOT_NULL };
	};

	struct MissionNeedsLot : public GameMsg {
		MissionNeedsLot() : GameMsg(MessageType::Game::MISSION_NEEDS_LOT) {}

		LOT item{};
	};

	struct PickupItem : public NetGameMsg {
		PickupItem() : NetGameMsg(MessageType::Game::PICKUP_ITEM) {}

		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
		bool Deserialize(RakNet::BitStream& stream) override;
		LWOOBJID lootID{};
		LWOOBJID lootOwnerID{};
	};
	using PickupItemEvent = NetGameMsgEvent<PickupItem>;

	struct TeamPickupItem : public NetGameMsg {
		TeamPickupItem() : NetGameMsg(MessageType::Game::TEAM_PICKUP_ITEM) {}

		void Serialize(RakNet::BitStream& stream) const override;
		LWOOBJID lootID{};
		LWOOBJID lootOwnerID{};
	};

	struct IsDead : public GameMsg {
		IsDead() : GameMsg(MessageType::Game::IS_DEAD) {}

		bool bDead{};
	};

	struct ToggleGMInvis : public NetGameMsg {
		ToggleGMInvis() : NetGameMsg(MessageType::Game::TOGGLE_GM_INVIS) {}

		void Serialize(RakNet::BitStream& stream) const override;
		bool bStateOut{ false };

	};
	using ToggleGMInvisEvent = NetGameMsgEvent<ToggleGMInvis>;

	struct GetGMInvis : public GameMsg {
		GetGMInvis() : GameMsg(MessageType::Game::GET_GM_INVIS) {}

		bool bGMInvis{ false };
  };

	struct ChildRemoved : public GameMsg {
		ChildRemoved() : GameMsg(MessageType::Game::CHILD_REMOVED) {}

		LWOOBJID childID{};
	};

	struct UseSkillSet : public NetGameMsg {
		UseSkillSet() : NetGameMsg(MessageType::Game::USE_SKILL_SET) {}
		void Serialize(RakNet::BitStream& bitStream) const override;

		bool bRemove{};
		LWOOBJID possessedId{ LWOOBJID_EMPTY };
		int32_t setId{ -1 };
	};

	struct ObjectLoaded : public GameMsg {
		ObjectLoaded() : GameMsg(MessageType::Game::OBJECT_LOADED) {}

		LWOOBJID objectID{};
		LOT lot{};
	};

	struct NotifyCombatAIStateChange : public GameMsg {
		NotifyCombatAIStateChange() : GameMsg(MessageType::Game::NOTIFY_COMBAT_AI_STATE_CHANGE) {}

		AiState newState{};
		AiState prevState{};
	};
};
#endif // GAMEMESSAGES_H
