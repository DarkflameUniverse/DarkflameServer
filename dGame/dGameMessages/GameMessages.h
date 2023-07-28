#ifndef GAMEMESSAGES_H
#define GAMEMESSAGES_H

#include "dCommonVars.h"
#include <map>
#include <string>
#include <vector>
#include "eMovementPlatformState.h"
#include "NiPoint3.h"
#include "eEndBehavior.h"
#include "eCyclingMode.h"
#include "eLootSourceType.h"
#include "Brick.h"

class AMFBaseValue;
class Entity;
class Item;
class NiQuaternion;
class User;
class Leaderboard;
class PropertySelectQueryProperty;
class TradeItem;

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
enum class ePetTamingNotifyType : uint32_t;
enum class eUseItemResponse : uint32_t;
enum class eQuickBuildFailReason : uint32_t;
enum class eRebuildState : uint32_t;

namespace GameMessages {
	class PropertyDataMessage;
	void SendFireEventClientSide(const LWOOBJID& objectID, const SystemAddress& sysAddr, std::u16string args, const LWOOBJID& object, int64_t param1, int param2, const LWOOBJID& sender);
	void SendTeleport(const LWOOBJID& objectID, const NiPoint3& pos, const NiQuaternion& rot, const SystemAddress& sysAddr, bool bSetRotation = false);
	void SendPlayAnimation(Entity* entity, const std::u16string& animationName, float fPriority = 0.0f, float fScale = 1.0f);
	void SendPlayerReady(Entity* entity, const SystemAddress& sysAddr);
	void SendPlayerAllowedRespawn(LWOOBJID entityID, bool doNotPromptRespawn, const SystemAddress& systemAddress);
	void SendInvalidZoneTransferList(Entity* entity, const SystemAddress& sysAddr, const std::u16string& feedbackURL, const std::u16string& invalidMapTransferList, bool feedbackOnExit, bool feedbackOnInvalidTransfer);
	void SendKnockback(const LWOOBJID& objectID, const LWOOBJID& caster, const LWOOBJID& originator, int knockBackTimeMS, const NiPoint3& vector);
	// https://lcdruniverse.org/lu_packets/lu_packets/world/gm/client/struct.VehicleStopBoost.html
	void SendVehicleStopBoost(Entity* targetEntity, const SystemAddress& playerSysAddr, bool affectPassive);
	void SendStartArrangingWithItem(
		Entity* entity,
		const SystemAddress& sysAddr,
		bool bFirstTime = true,
		const LWOOBJID& buildAreaID = LWOOBJID_EMPTY,
		NiPoint3 buildStartPOS = NiPoint3::ZERO,
		int sourceBAG = 0,
		const LWOOBJID& sourceID = LWOOBJID_EMPTY,
		LOT sourceLOT = 0,
		int sourceTYPE = 8,
		const LWOOBJID& targetID = 0,
		LOT targetLOT = 0,
		NiPoint3 targetPOS = NiPoint3::ZERO,
		int targetTYPE = 0
	);

	void SendPlayerSetCameraCyclingMode(const LWOOBJID& objectID, const SystemAddress& sysAddr, bool bAllowCyclingWhileDeadOnly = true, eCyclingMode cyclingMode = eCyclingMode::ALLOW_CYCLE_TEAMMATES);

	void SendPlayNDAudioEmitter(Entity* entity, const SystemAddress& sysAddr, std::string audioGUID);

	void SendStartPathing(Entity* entity);
	void SendPlatformResync(Entity* entity, const SystemAddress& sysAddr, bool bStopAtDesiredWaypoint = false,
		int iIndex = 0, int iDesiredWaypointIndex = 1, int nextIndex = 1,
		eMovementPlatformState movementState = eMovementPlatformState::Moving);

	void SendRestoreToPostLoadStats(Entity* entity, const SystemAddress& sysAddr);
	void SendServerDoneLoadingAllObjects(Entity* entity, const SystemAddress& sysAddr);
	void SendGMLevelBroadcast(const LWOOBJID& objectID, eGameMasterLevel level);
	void SendChatModeUpdate(const LWOOBJID& objectID, eGameMasterLevel level);

	void SendAddItemToInventoryClientSync(Entity* entity, const SystemAddress& sysAddr, Item* item, const LWOOBJID& objectID, bool showFlyingLoot, int itemCount, LWOOBJID subKey = LWOOBJID_EMPTY, eLootSourceType lootSourceType = eLootSourceType::NONE);
	void SendNotifyClientFlagChange(const LWOOBJID& objectID, uint32_t iFlagID, bool bFlag, const SystemAddress& sysAddr);
	void SendChangeObjectWorldState(const LWOOBJID& objectID, eObjectWorldState state, const SystemAddress& sysAddr);

	void SendOfferMission(const LWOOBJID& entity, const SystemAddress& sysAddr, int32_t missionID, const LWOOBJID& offererID);
	void SendNotifyMission(Entity* entity, const SystemAddress& sysAddr, int missionID, int missionState, bool sendingRewards);
	void SendNotifyMissionTask(Entity* entity, const SystemAddress& sysAddr, int missionID, int taskMask, std::vector<float> updates);
	void NotifyLevelRewards(LWOOBJID objectID, const SystemAddress& sysAddr, int level, bool sending_rewards);

	void SendModifyLEGOScore(Entity* entity, const SystemAddress& sysAddr, int64_t score, eLootSourceType sourceType);
	void SendUIMessageServerToSingleClient(Entity* entity, const SystemAddress& sysAddr, const std::string& message, AMFBaseValue& args);
	void SendUIMessageServerToAllClients(const std::string& message, AMFBaseValue& args);

	void SendPlayEmbeddedEffectOnAllClientsNearObject(Entity* entity, std::u16string effectName, const LWOOBJID& fromObjectID, float radius);
	void SendPlayFXEffect(Entity* entity, int32_t effectID, const std::u16string& effectType, const std::string& name, LWOOBJID secondary, float priority = 1, float scale = 1, bool serialize = true);
	void SendPlayFXEffect(const LWOOBJID& entity, int32_t effectID, const std::u16string& effectType, const std::string& name, LWOOBJID secondary = LWOOBJID_EMPTY, float priority = 1, float scale = 1, bool serialize = true);
	void SendStopFXEffect(Entity* entity, bool killImmediate, std::string name);
	void SendBroadcastTextToChatbox(Entity* entity, const SystemAddress& sysAddr, const std::u16string& attrs, const std::u16string& wsText);
	void SendSetCurrency(Entity* entity, int64_t currency, int lootType, const LWOOBJID& sourceID, const LOT& sourceLOT, int sourceTradeID, bool overrideCurrent, eLootSourceType sourceType);

	void SendRebuildNotifyState(Entity* entity, eRebuildState prevState, eRebuildState state, const LWOOBJID& playerID);
	void SendEnableRebuild(Entity* entity, bool enable, bool fail, bool success, eQuickBuildFailReason failReason, float duration, const LWOOBJID& playerID);
	void AddActivityOwner(Entity* entity, LWOOBJID& ownerID);
	void SendTerminateInteraction(const LWOOBJID& objectID, eTerminateType type, const LWOOBJID& terminator);

	void SendDieNoImplCode(Entity* entity, const LWOOBJID& killerID, const LWOOBJID& lootOwnerID, eKillType killType, std::u16string deathType, float directionRelative_AngleY, float directionRelative_AngleXZ, float directionRelative_Force, bool bClientDeath, bool bSpawnLoot);
	void SendDie(Entity* entity, const LWOOBJID& killerID, const LWOOBJID& lootOwnerID, bool bDieAccepted, eKillType killType, std::u16string deathType, float directionRelative_AngleY, float directionRelative_AngleXZ, float directionRelative_Force, bool bClientDeath, bool bSpawnLoot, float coinSpawnTime);

	void SendSetInventorySize(Entity* entity, int invType, int size);

	void SendSetEmoteLockState(Entity* entity, bool bLock, int emoteID);
	void SendSetJetPackMode(Entity* entity, bool use, bool bypassChecks = false, bool doHover = false, int effectID = -1, float airspeed = 10, float maxAirspeed = 15, float verticalVelocity = 1, int warningEffectID = -1);
	void SendResurrect(Entity* entity);
	void SendStop2DAmbientSound(Entity* entity, bool force, std::string audioGUID, bool result = false);
	void SendPlay2DAmbientSound(Entity* entity, std::string audioGUID, bool result = false);
	void SendSetNetworkScriptVar(Entity* entity, const SystemAddress& sysAddr, std::string data);
	void SendDropClientLoot(Entity* entity, const LWOOBJID& sourceID, LOT item, int currency, NiPoint3 spawnPos = NiPoint3::ZERO, int count = 1);

	void SendSetPlayerControlScheme(Entity* entity, eControlScheme controlScheme);
	void SendPlayerReachedRespawnCheckpoint(Entity* entity, const NiPoint3& position, const NiQuaternion& rotation);

	void SendAddSkill(Entity* entity, TSkillID skillID, int slotID);
	void SendRemoveSkill(Entity* entity, TSkillID skillID);

	void SendFinishArrangingWithItem(Entity* entity, const LWOOBJID& buildAreaID);
	void SendModularBuildEnd(Entity* entity);

	void SendVendorOpenWindow(Entity* entity, const SystemAddress& sysAddr);
	void SendVendorStatusUpdate(Entity* entity, const SystemAddress& sysAddr, bool bUpdateOnly = false);
	void SendVendorTransactionResult(Entity* entity, const SystemAddress& sysAddr);

	void SendRemoveItemFromInventory(Entity* entity, const SystemAddress& sysAddr, LWOOBJID iObjID, LOT templateID, int inventoryType, uint32_t stackCount, uint32_t stackRemaining);
	void SendConsumeClientItem(Entity* entity, bool bSuccess, LWOOBJID item);
	void SendUseItemResult(Entity* entity, LOT templateID, bool useItemResult);
	void SendMoveInventoryBatch(Entity* entity, uint32_t stackCount, int srcInv, int dstInv, const LWOOBJID& iObjID);

	void SendMatchResponse(Entity* entity, const SystemAddress& sysAddr, int response);
	void SendMatchUpdate(Entity* entity, const SystemAddress& sysAddr, std::string data, eMatchUpdate type);

	void HandleUnUseModel(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void SendStartCelebrationEffect(Entity* entity, const SystemAddress& sysAddr, int celebrationID);

	// https://lcdruniverse.org/lu_packets/lu_packets/world/gm/client/struct.SetResurrectRestoreValues.html
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

	/**
	 * @brief This GameMessage is the one that handles all of the property behavior incoming messages from the client.
	 *
	 * The GameMessage struct can be located here https://lcdruniverse.org/lu_packets/lu_packets/world/gm/server/struct.ControlBehaviors.html
	 * For information on the AMF3 format can be found here https://rtmp.veriskope.com/pdf/amf3-file-format-spec.pdf
	 * For any questions regarding AMF3 you can contact EmosewaMC on GitHub
	 *
	 * @param inStream The incoming data sent from the client
	 * @param entity The Entity that sent the message
	 * @param sysAddr The SystemAddress that sent the message
	 */
	void HandleControlBehaviors(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

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

	void HandleClientRailMovementReady(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleCancelRailMovement(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandlePlayerRailArrivedNotification(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendNotifyClientObject(const LWOOBJID& objectID, std::u16string name, int param1 = 0, int param2 = 0, const LWOOBJID& paramObj = LWOOBJID_EMPTY, std::string paramStr = "", const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);
	void SendNotifyClientZoneObject(const LWOOBJID& objectID, const std::u16string& name, int param1, int param2, const LWOOBJID& paramObj, const std::string& paramStr, const SystemAddress& sysAddr);

	void SendNotifyClientFailedPrecondition(LWOOBJID objectId, const SystemAddress& sysAddr, const std::u16string& failedReason, int preconditionID);

	// The success or failure response sent back to the client will preserve the same value for localID.
	void SendBBBSaveResponse(const LWOOBJID& objectId, const LWOOBJID& localID, unsigned char* buffer, uint32_t bufferSize, const SystemAddress& sysAddr);

	void SendAddBuff(LWOOBJID& objectID, const LWOOBJID& casterID, uint32_t buffID, uint32_t msDuration,
		bool addImmunity = false, bool cancelOnDamaged = false, bool cancelOnDeath = true,
		bool cancelOnLogout = false, bool cancelOnRemoveBuff = true, bool cancelOnUi = false,
		bool cancelOnUnequip = false, bool cancelOnZone = false, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);

	void SendToggleGMInvis(LWOOBJID objectId, bool enabled, const SystemAddress& sysAddr);

	void SendSetName(LWOOBJID objectID, std::u16string name, const SystemAddress& sysAddr);

	// Property messages

	void SendOpenPropertyVendor(LWOOBJID objectId, const SystemAddress& sysAddr);

	void SendOpenPropertyManagment(LWOOBJID objectId, const SystemAddress& sysAddr);

	void SendDownloadPropertyData(LWOOBJID objectId, const PropertyDataMessage& data, const SystemAddress& sysAddr);

	/**
	 * @brief Send an updated item id to the client when they load a blueprint in brick build mode
	 *
	 * @param sysAddr SystemAddress to respond to
	 * @param oldItemId The item ID that was requested to be loaded
	 * @param newItemId The new item ID of the loaded item
	 *
	 */
	void SendBlueprintLoadItemResponse(const SystemAddress& sysAddr, bool success, LWOOBJID oldItemId, LWOOBJID newItemId);

	void SendPropertyRentalResponse(LWOOBJID objectId, LWOCLONEID cloneId, uint32_t code, LWOOBJID propertyId, int64_t rentDue, const SystemAddress& sysAddr);

	void SendLockNodeRotation(Entity* entity, std::string nodeName);

	void SendSetBuildModeConfirmed(LWOOBJID objectId, const SystemAddress& sysAddr, bool start, bool warnVisitors, bool modePaused, int32_t modeValue, LWOOBJID playerId, NiPoint3 startPos = NiPoint3::ZERO);

	void SendGetModelsOnProperty(LWOOBJID objectId, std::map<LWOOBJID, LWOOBJID> models, const SystemAddress& sysAddr);

	void SendZonePropertyModelEquipped(LWOOBJID objectId, LWOOBJID playerId, LWOOBJID propertyId, const SystemAddress& sysAddr);

	void SendPlaceModelResponse(LWOOBJID objectId, const SystemAddress& sysAddr, NiPoint3 position, LWOOBJID plaque, int32_t response, NiQuaternion rotation);

	void SendUGCEquipPreCreateBasedOnEditMode(LWOOBJID objectId, const SystemAddress& sysAddr, int modelCount, LWOOBJID model);

	void SendUGCEquipPostDeleteBasedOnEditMode(LWOOBJID objectId, const SystemAddress& sysAddr, LWOOBJID inventoryItem, int itemTotal);

	void HandleSetPropertyAccess(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleUpdatePropertyOrModelForFilterCheck(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleQueryPropertyData(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleSetBuildMode(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleStartBuildingWithItem(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandlePropertyEditorBegin(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandlePropertyEditorEnd(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandlePropertyContentsFromClient(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandlePropertyModelEquipped(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandlePlacePropertyModel(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleUpdatePropertyModel(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleDeletePropertyModel(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleBBBLoadItemRequest(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleBBBSaveRequest(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandlePropertyEntranceSync(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleEnterProperty(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleSetConsumableItem(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendPlayCinematic(LWOOBJID objectId, std::u16string pathName, const SystemAddress& sysAddr,
		bool allowGhostUpdates = true, bool bCloseMultiInteract = true, bool bSendServerNotify = false, bool bUseControlledObjectForAudioListener = false,
		eEndBehavior endBehavior = eEndBehavior::RETURN, bool hidePlayerDuringCine = false, float leadIn = -1, bool leavePlayerLockedWhenFinished = false,
		bool lockPlayer = true, bool result = false, bool skipIfSamePath = false, float startTimeAdvance = 0);

	void SendEndCinematic(LWOOBJID objectID, std::u16string pathName, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS,
		float leadOut = -1.0f, bool leavePlayerLocked = false);
	void HandleCinematicUpdate(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

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

	void SendPropertyEntranceBegin(LWOOBJID objectId, const SystemAddress& sysAddr);

	void SendPropertySelectQuery(LWOOBJID objectId, int32_t navOffset, bool thereAreMore, int32_t cloneId, bool hasFeaturedProperty, bool wasFriends, const std::vector<PropertySelectQueryProperty>& entries, const SystemAddress& sysAddr);

	void SendNotifyObject(LWOOBJID objectId, LWOOBJID objIDSender, std::u16string name, const SystemAddress& sysAddr, int param1 = 0, int param2 = 0);

	void HandleVerifyAck(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendTeamPickupItem(LWOOBJID objectId, LWOOBJID lootID, LWOOBJID lootOwnerID, const SystemAddress& sysAddr);

	//Trading:
	void SendServerTradeInvite(LWOOBJID objectId, bool bNeedInvitePopUp, LWOOBJID i64Requestor, std::u16string wsName, const SystemAddress& sysAddr);

	void SendServerTradeInitialReply(LWOOBJID objectId, LWOOBJID i64Invitee, int32_t resultType, std::u16string wsName, const SystemAddress& sysAddr);

	void SendServerTradeFinalReply(LWOOBJID objectId, bool bResult, LWOOBJID i64Invitee, std::u16string wsName, const SystemAddress& sysAddr);

	void SendServerTradeAccept(LWOOBJID objectId, bool bFirst, const SystemAddress& sysAddr);

	void SendServerTradeCancel(LWOOBJID objectId, const SystemAddress& sysAddr);

	void SendServerTradeUpdate(LWOOBJID objectId, uint64_t coins, const std::vector<TradeItem>& items, const SystemAddress& sysAddr);

	void HandleClientTradeRequest(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleClientTradeCancel(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleClientTradeAccept(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleClientTradeUpdate(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	//Pets:
	void SendNotifyPetTamingMinigame(LWOOBJID objectId, LWOOBJID petId, LWOOBJID playerTamingId, bool bForceTeleport, ePetTamingNotifyType notifyType, NiPoint3 petsDestPos, NiPoint3 telePos, NiQuaternion teleRot, const SystemAddress& sysAddr);

	void SendNotifyPetTamingPuzzleSelected(LWOOBJID objectId, const std::vector<Brick>& bricks, const SystemAddress& sysAddr);

	void SendNotifyTamingModelLoadedOnServer(LWOOBJID objectId, const SystemAddress& sysAddr);

	void SendPetTamingTryBuildResult(LWOOBJID objectId, bool bSuccess, int32_t iNumCorrect, const SystemAddress& sysAddr);

	void SendPetResponse(LWOOBJID objectId, LWOOBJID objIDPet, int32_t iPetCommandType, int32_t iResponse, int32_t iTypeID, const SystemAddress& sysAddr);

	void SendAddPetToPlayer(LWOOBJID objectId, int32_t iElementalType, std::u16string name, LWOOBJID petDBID, LOT petLOT, const SystemAddress& sysAddr);

	void SendRegisterPetID(LWOOBJID objectId, LWOOBJID objID, const SystemAddress& sysAddr);

	void SendRegisterPetDBID(LWOOBJID objectId, LWOOBJID petDBID, const SystemAddress& sysAddr);

	void SendMarkInventoryItemAsActive(LWOOBJID objectId, bool bActive, eUnequippableActiveType iType, LWOOBJID itemID, const SystemAddress& sysAddr);

	void SendClientExitTamingMinigame(LWOOBJID objectId, bool bVoluntaryExit, const SystemAddress& sysAddr);

	void SendShowPetActionButton(LWOOBJID objectId, int32_t buttonLabel, bool bShow, const SystemAddress& sysAddr);

	void SendPlayEmote(LWOOBJID objectId, int32_t emoteID, LWOOBJID target, const SystemAddress& sysAddr);

	void SendBouncerActiveStatus(LWOOBJID objectId, bool bActive, const SystemAddress& sysAddr);

	void SendSetPetName(LWOOBJID objectId, std::u16string name, LWOOBJID petDBID, const SystemAddress& sysAddr);

	void SendSetPetNameModerated(LWOOBJID objectId, LWOOBJID petDBID, int32_t nModerationStatus, const SystemAddress& sysAddr);

	void SendPetNameChanged(LWOOBJID objectId, int32_t moderationStatus, std::u16string name, std::u16string ownerName, const SystemAddress& sysAddr);

	void HandleClientExitTamingMinigame(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleStartServerPetMinigameTimer(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandlePetTamingTryBuild(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleNotifyTamingBuildSuccess(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleRequestSetPetName(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleCommandPet(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleDespawnPet(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleMessageBoxResponse(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleChoiceBoxRespond(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendDisplayZoneSummary(LWOOBJID objectId, const SystemAddress& sysAddr, bool isPropertyMap = false, bool isZoneStart = false, LWOOBJID sender = LWOOBJID_EMPTY);

	//UI:
	void SendNotifyNotEnoughInvSpace(LWOOBJID objectId, uint32_t freeSlotsNeeded, eInventoryType inventoryType, const SystemAddress& sysAddr);

	void SendDisplayMessageBox(LWOOBJID objectId, bool bShow, LWOOBJID callbackClient, const std::u16string& identifier, int32_t imageID, const std::u16string& text, const std::u16string& userData, const SystemAddress& sysAddr);

	void SendDisplayChatBubble(LWOOBJID objectId, const std::u16string& text, const SystemAddress& sysAddr);

	/**
	 * @brief
	 *
	 * @param objectId ID of the entity to set idle flags
	 * @param FlagsOn Flag to turn on
	 * @param FlagsOff Flag to turn off
	 */
	void SendChangeIdleFlags(LWOOBJID objectId, eAnimationFlags FlagsOn, eAnimationFlags FlagsOff, const SystemAddress& sysAddr);

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
	void HandleDismountComplete(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	/**
	 * @brief Handle acknowledging that the client possessed something
	 *
	 * @param inStream Raknet BitStream of incoming data
	 * @param entity The Entity that is possessing
	 * @param sysAddr the system address to send game message responses to
	 */
	void HandleAcknowledgePossession(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	/**
	 * @brief A request from a client to get the hot properties that would appear on the news feed
	 * This incoming message has NO DATA and is simply a request that expects to send a reply to the sender.
	 *
	 * @param inStream packet of data
	 * @param entity The Entity that sent the request
	 * @param sysAddr The SystemAddress of the Entity that sent the request
	 */
	void HandleGetHotPropertyData(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	/**
	 * @brief A request from a client to get the hot properties that would appear on the news feed
	 * The struct of data to send is as follows
	 *
	 * [u32] - Number of properties
	 *     [objid] - property id
	 *     [objid] - property owner id
	 *     [wstring] - property owner name
	 *     [u64] - total reputation
	 *     [i32] - property template id
	 *     [wstring] - property name
	 *     [wstring] - property description
	 *     [float] - performance cost
	 *     [timestamp] - time last published
	 *     [cloneid] - clone id
	 *
	 * @param inStream packet of data
	 * @param entity The Entity that sent the request
	 * @param sysAddr The SystemAddress of the Entity that sent the request
	 */
	void SendGetHotPropertyData(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	//Racing:
	void HandleModuleAssemblyQueryData(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleModularAssemblyNIFCompleted(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleVehicleSetWheelLockState(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleRacingClientReady(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleRequestDie(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleVehicleNotifyServerAddPassiveBoostAction(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleVehicleNotifyServerRemovePassiveBoostAction(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleRacingPlayerInfoResetFinished(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void HandleVehicleNotifyHitImaginationServer(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendModuleAssemblyDBDataForClient(LWOOBJID objectId, LWOOBJID assemblyID, const std::u16string& data, const SystemAddress& sysAddr);

	void SendNotifyVehicleOfRacingObject(LWOOBJID objectId, LWOOBJID racingObjectID, const SystemAddress& sysAddr);

	void SendRacingPlayerLoaded(LWOOBJID objectId, LWOOBJID playerID, LWOOBJID vehicleID, const SystemAddress& sysAddr);

	void SendVehicleUnlockInput(LWOOBJID objectId, bool bLockWheels, const SystemAddress& sysAddr);

	void SendVehicleSetWheelLockState(LWOOBJID objectId, bool bExtraFriction, bool bLocked, const SystemAddress& sysAddr);

	void SendRacingSetPlayerResetInfo(LWOOBJID objectId, int32_t currentLap, uint32_t furthestResetPlane, LWOOBJID playerID, NiPoint3 respawnPos, uint32_t upcomingPlane, const SystemAddress& sysAddr);

	void SendRacingResetPlayerToLastReset(LWOOBJID objectId, LWOOBJID playerID, const SystemAddress& sysAddr);

	void SendNotifyRacingClient(LWOOBJID objectId, int32_t eventType, int32_t param1, LWOOBJID paramObj, std::u16string paramStr, LWOOBJID singleClient, const SystemAddress& sysAddr);

	void SendActivityEnter(LWOOBJID objectId, const SystemAddress& sysAddr);
	void SendActivityStart(LWOOBJID objectId, const SystemAddress& sysAddr);
	void SendActivityExit(LWOOBJID objectId, const SystemAddress& sysAddr);
	void SendActivityStop(LWOOBJID objectId, bool bExit, bool bUserCancel, const SystemAddress& sysAddr);
	void SendActivityPause(LWOOBJID objectId, bool pause = false, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);
	void SendStartActivityTime(LWOOBJID objectId, float_t startTime, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);
	void SendRequestActivityEnter(LWOOBJID objectId, const SystemAddress& sysAddr, bool bStart, LWOOBJID userID);
	void SendUseItemRequirementsResponse(LWOOBJID objectID, const SystemAddress& sysAddr, eUseItemResponse itemResponse);

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

	void HandleUpdatePropertyPerformanceCost(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendNotifyClientShootingGalleryScore(LWOOBJID objectId, const SystemAddress& sysAddr,
		float addTime,
		int32_t score,
		LWOOBJID target,
		NiPoint3 targetPos
	);

	void HandleUpdateShootingGalleryRotation(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendUpdateReputation(const LWOOBJID objectId, const int64_t reputation, const SystemAddress& sysAddr);

	// Leaderboards
	void SendActivitySummaryLeaderboardData(const LWOOBJID& objectID, const Leaderboard* leaderboard,
		const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);
	void HandleActivitySummaryLeaderboardData(RakNet::BitStream* instream, Entity* entity, const SystemAddress& sysAddr);
	void SendRequestActivitySummaryLeaderboardData(const LWOOBJID& objectID, const LWOOBJID& targetID,
		const SystemAddress& sysAddr, const int32_t& gameID = 0,
		const int32_t& queryType = 1, const int32_t& resultsEnd = 10,
		const int32_t& resultsStart = 0, bool weekly = false);
	void HandleRequestActivitySummaryLeaderboardData(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleActivityStateChangeRequest(RakNet::BitStream* inStream, Entity* entity);

	void SendVehicleAddPassiveBoostAction(LWOOBJID objectId, const SystemAddress& sysAddr);

	void SendVehicleRemovePassiveBoostAction(LWOOBJID objectId, const SystemAddress& sysAddr);

	void SendVehicleNotifyFinishedRace(LWOOBJID objectId, const SystemAddress& sysAddr);

	//NT:

	void HandleRequestMoveItemBetweenInventoryTypes(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendShowActivityCountdown(LWOOBJID objectId, bool bPlayAdditionalSound, bool bPlayCountdownSound, std::u16string sndName, int32_t stateToPlaySoundOn, const SystemAddress& sysAddr);

	//Handlers:

	void HandleToggleGhostReferenceOverride(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleSetGhostReferencePosition(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);

	void SendSetNamebillboardState(const SystemAddress& sysAddr, LWOOBJID objectId);
	void SendShowBillboardInteractIcon(const SystemAddress& sysAddr, LWOOBJID objectId);
	void HandleBuyFromVendor(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleSellToVendor(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleBuybackFromVendor(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleParseChatMessage(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleToggleGhostReffrenceOverride(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleSetGhostReffrenceOverride(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleFireEventServerSide(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleRequestPlatformResync(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleRebuildCancel(RakNet::BitStream* inStream, Entity* entity);
	void HandleRequestUse(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandlePlayEmote(RakNet::BitStream* inStream, Entity* entity);
	void HandleModularBuildConvertModel(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleSetFlag(RakNet::BitStream* inStream, Entity* entity);
	void HandleRespondToMission(RakNet::BitStream* inStream, Entity* entity);
	void HandleMissionDialogOK(RakNet::BitStream* inStream, Entity* entity);
	void HandleRequestLinkedMission(RakNet::BitStream* inStream, Entity* entity);
	void HandleHasBeenCollected(RakNet::BitStream* inStream, Entity* entity);
	void HandleNotifyServerLevelProcessingComplete(RakNet::BitStream* inStream, Entity* entity);
	void HandlePickupCurrency(RakNet::BitStream* inStream, Entity* entity);
	void HandleRequestDie(RakNet::BitStream* inStream, Entity* entity);
	void HandleEquipItem(RakNet::BitStream* inStream, Entity* entity);
	void HandleUnequipItem(RakNet::BitStream* inStream, Entity* entity);
	void HandleRemoveItemFromInventory(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleMoveItemInInventory(RakNet::BitStream* inStream, Entity* entity);
	void HandleMoveItemBetweenInventoryTypes(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleBuildModeSet(RakNet::BitStream* inStream, Entity* entity);
	void HandleModularBuildFinish(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleDoneArrangingWithItem(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandleModularBuildMoveAndEquip(RakNet::BitStream* inStream, Entity* entity, const SystemAddress& sysAddr);
	void HandlePickupItem(RakNet::BitStream* inStream, Entity* entity);
	void HandleResurrect(RakNet::BitStream* inStream, Entity* entity);
	void HandleModifyPlayerZoneStatistic(RakNet::BitStream* inStream, Entity* entity);
	void HandleUpdatePlayerStatistic(RakNet::BitStream* inStream, Entity* entity);

	void HandlePushEquippedItemsState(RakNet::BitStream* inStream, Entity* entity);

	void HandlePopEquippedItemsState(RakNet::BitStream* inStream, Entity* entity);

	void HandleClientItemConsumed(RakNet::BitStream* inStream, Entity* entity);

	void HandleUseNonEquipmentItem(RakNet::BitStream* inStream, Entity* entity);

	void HandleMatchRequest(RakNet::BitStream* inStream, Entity* entity);

	void HandleReportBug(RakNet::BitStream* inStream, Entity* entity);

	void SendRemoveBuff(Entity* entity, bool fromUnEquip, bool removeImmunity, uint32_t buffId);

	// bubble
	void HandleDeactivateBubbleBuff(RakNet::BitStream* inStream, Entity* entity);

	void HandleActivateBubbleBuff(RakNet::BitStream* inStream, Entity* entity);

	void SendActivateBubbleBuffFromServer(LWOOBJID objectId, const SystemAddress& sysAddr);

	void SendDeactivateBubbleBuffFromServer(LWOOBJID objectId, const SystemAddress& sysAddr);

	void HandleZoneSummaryDismissed(RakNet::BitStream* inStream, Entity* entity);
	void HandleRequestActivityExit(RakNet::BitStream* inStream, Entity* entity);
};

#endif // GAMEMESSAGES_H
