/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "GameMessageHandler.h"
#include "MissionComponent.h"
#include "PacketUtils.h"
#include "dServer.h"
#include "../thirdparty/raknet/Source/RakNetworkFactory.h"
#include <future>
#include "User.h"
#include "UserManager.h"
#include "BitStream.h"
#include "RakPeer.h"
#include "DestroyableComponent.h"
#include "InventoryComponent.h"
#include "Character.h"
#include "ControllablePhysicsComponent.h"
#include "dZoneManager.h"
#include "Player.h"
#include "CppScripts.h"

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDSkillBehaviorTable.h"
#include "SkillComponent.h"
#include "RacingControlComponent.h"
#include "RequestServerProjectileImpact.h"
#include "SyncSkill.h"
#include "StartSkill.h"
#include "EchoStartSkill.h"
#include "EchoSyncSkill.h"
#include "eMissionTaskType.h"
#include "eReplicaComponentType.h"
#include "eConnectionType.h"

using namespace std;

void GameMessageHandler::HandleMessage(RakNet::BitStream* inStream, const SystemAddress& sysAddr, LWOOBJID objectID, eGameMessageType messageID) {

	CBITSTREAM;

	// Get the entity
	Entity* entity = Game::entityManager->GetEntity(objectID);

	User* usr = UserManager::Instance()->GetUser(sysAddr);

	if (!entity) {
		Game::logger->Log("GameMessageHandler", "Failed to find associated entity (%llu), aborting GM (%X)!", objectID, messageID);

		return;
	}

	Game::logger->LogDebug("GameMessageHandler", "received game message ID: %i", messageID);
	switch (messageID) {

	case eGameMessageType::UN_USE_BBB_MODEL: {
		GameMessages::HandleUnUseModel(inStream, entity, sysAddr);
		break;
	}
	case eGameMessageType::PLAY_EMOTE: {
		GameMessages::HandlePlayEmote(inStream, entity);
		break;
	}

	case eGameMessageType::MOVE_ITEM_IN_INVENTORY: {
		GameMessages::HandleMoveItemInInventory(inStream, entity);
		break;
	}

	case eGameMessageType::REMOVE_ITEM_FROM_INVENTORY: {
		GameMessages::HandleRemoveItemFromInventory(inStream, entity, sysAddr);
		break;
	}

	case eGameMessageType::EQUIP_ITEM:
		GameMessages::HandleEquipItem(inStream, entity);
		break;

	case eGameMessageType::UN_EQUIP_ITEM:
		GameMessages::HandleUnequipItem(inStream, entity);
		break;

	case eGameMessageType::RESPOND_TO_MISSION: {
		GameMessages::HandleRespondToMission(inStream, entity);
		break;
	}

	case eGameMessageType::REQUEST_USE: {
		GameMessages::HandleRequestUse(inStream, entity, sysAddr);
		break;
	}

	case eGameMessageType::SET_FLAG: {
		GameMessages::HandleSetFlag(inStream, entity);
		break;
	}

	case eGameMessageType::HAS_BEEN_COLLECTED: {
		GameMessages::HandleHasBeenCollected(inStream, entity);
		break;
	}

	case eGameMessageType::PLAYER_LOADED: {
		GameMessages::SendRestoreToPostLoadStats(entity, sysAddr);
		entity->SetPlayerReadyForUpdates();

		auto* player = dynamic_cast<Player*>(entity);
		if (player != nullptr) {
			player->ConstructLimboEntities();
		}

		InventoryComponent* inv = entity->GetComponent<InventoryComponent>();
		if (inv) {
			auto items = inv->GetEquippedItems();
			for (auto pair : items) {
				const auto item = pair.second;

				inv->AddItemSkills(item.lot);
			}
		}

		auto* destroyable = entity->GetComponent<DestroyableComponent>();
		destroyable->SetImagination(destroyable->GetImagination());
		Game::entityManager->SerializeEntity(entity);

		std::vector<Entity*> racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
		for (Entity* racingController : racingControllers) {
			auto* racingComponent = racingController->GetComponent<RacingControlComponent>();
			if (racingComponent != nullptr) {
				racingComponent->OnPlayerLoaded(entity);
			}
		}

		Entity* zoneControl = Game::entityManager->GetZoneControlEntity();
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(zoneControl)) {
			script->OnPlayerLoaded(zoneControl, player);
		}

		std::vector<Entity*> scriptedActs = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::SCRIPT);
		for (Entity* scriptEntity : scriptedActs) {
			if (scriptEntity->GetObjectID() != zoneControl->GetObjectID()) { // Don't want to trigger twice on instance worlds
				for (CppScripts::Script* script : CppScripts::GetEntityScripts(scriptEntity)) {
					script->OnPlayerLoaded(scriptEntity, player);
				}
			}
		}

		//Kill player if health == 0
		if (entity->GetIsDead()) {
			entity->Smash(entity->GetObjectID());
		}

		//if the player has moved significantly, move them back:
		if ((entity->GetPosition().y - entity->GetCharacter()->GetOriginalPos().y) > 2.0f) {
			// Disabled until fixed
			//GameMessages::SendTeleport(entity->GetObjectID(), entity->GetCharacter()->GetOriginalPos(), entity->GetCharacter()->GetOriginalRot(), entity->GetSystemAddress(), true, true);
		}

		/**
		 * Invoke the OnZoneLoad event on the player character
		 */
		auto* character = entity->GetCharacter();

		if (character != nullptr) {
			character->OnZoneLoad();
		}

		Game::logger->Log("GameMessageHandler", "Player %s (%llu) loaded.", entity->GetCharacter()->GetName().c_str(), entity->GetObjectID());

		// After we've done our thing, tell the client they're ready
		GameMessages::SendPlayerReady(entity, sysAddr);
		GameMessages::SendPlayerReady(Game::zoneManager->GetZoneControlObject(), sysAddr);

		break;
	}

	case eGameMessageType::REQUEST_LINKED_MISSION: {
		GameMessages::HandleRequestLinkedMission(inStream, entity);
		break;
	}

	case eGameMessageType::MISSION_DIALOGUE_OK: {
		GameMessages::HandleMissionDialogOK(inStream, entity);
		break;
	}

	case eGameMessageType::MISSION_DIALOGUE_CANCELLED: {
		//This message is pointless for our implementation, as the client just carries on after
		//rejecting a mission offer. We dont need to do anything. This is just here to remove a warning in our logs :)
		break;
	}

	case eGameMessageType::REQUEST_PLATFORM_RESYNC: {
		GameMessages::HandleRequestPlatformResync(inStream, entity, sysAddr);
		break;
	}

	case eGameMessageType::FIRE_EVENT_SERVER_SIDE: {
		GameMessages::HandleFireEventServerSide(inStream, entity, sysAddr);
		break;
	}

	case eGameMessageType::SEND_ACTIVITY_SUMMARY_LEADERBOARD_DATA: {
		GameMessages::HandleActivitySummaryLeaderboardData(inStream, entity, sysAddr);
		break;
	}

	case eGameMessageType::REQUEST_ACTIVITY_SUMMARY_LEADERBOARD_DATA: {
		GameMessages::HandleRequestActivitySummaryLeaderboardData(inStream, entity, sysAddr);
		break;
	}

	case eGameMessageType::ACTIVITY_STATE_CHANGE_REQUEST: {
		GameMessages::HandleActivityStateChangeRequest(inStream, entity);
		break;
	}

	case eGameMessageType::PARSE_CHAT_MESSAGE: {
		GameMessages::HandleParseChatMessage(inStream, entity, sysAddr);
		break;
	}

	case eGameMessageType::NOTIFY_SERVER_LEVEL_PROCESSING_COMPLETE: {
		GameMessages::HandleNotifyServerLevelProcessingComplete(inStream, entity);
		break;
	}

	case eGameMessageType::PICKUP_CURRENCY: {
		GameMessages::HandlePickupCurrency(inStream, entity);
		break;
	}

	case eGameMessageType::PICKUP_ITEM: {
		GameMessages::HandlePickupItem(inStream, entity);
		break;
	}

	case eGameMessageType::RESURRECT: {
		GameMessages::HandleResurrect(inStream, entity);
		break;
	}

	case eGameMessageType::REQUEST_RESURRECT: {
		GameMessages::SendResurrect(entity);
		/*auto* dest = static_cast<DestroyableComponent*>(entity->GetComponent(eReplicaComponentType::DESTROYABLE));
		if (dest) {
			dest->SetHealth(4);
			dest->SetArmor(0);
			dest->SetImagination(6);
			Game::entityManager->SerializeEntity(entity);
		}*/
		break;
	}
	case eGameMessageType::HANDLE_HOT_PROPERTY_DATA: {
		GameMessages::HandleGetHotPropertyData(inStream, entity, sysAddr);
		break;
	}

	case eGameMessageType::REQUEST_SERVER_PROJECTILE_IMPACT:
	{
		auto message = RequestServerProjectileImpact();

		message.Deserialize(inStream);

		auto* skill_component = entity->GetComponent<SkillComponent>();

		if (skill_component != nullptr) {
			auto* bs = new RakNet::BitStream((unsigned char*)message.sBitStream.c_str(), message.sBitStream.size(), false);

			skill_component->SyncPlayerProjectile(message.i64LocalID, bs, message.i64TargetID);

			delete bs;
		}

		break;
	}

	case eGameMessageType::START_SKILL: {
		StartSkill startSkill = StartSkill();
		startSkill.Deserialize(inStream); // inStream replaces &bitStream

		if (startSkill.skillID == 1561 || startSkill.skillID == 1562 || startSkill.skillID == 1541) return;

		MissionComponent* comp = entity->GetComponent<MissionComponent>();
		if (comp) {
			comp->Progress(eMissionTaskType::USE_SKILL, startSkill.skillID);
		}

		CDSkillBehaviorTable* skillTable = CDClientManager::Instance().GetTable<CDSkillBehaviorTable>();
		unsigned int behaviorId = skillTable->GetSkillByID(startSkill.skillID).behaviorID;

		bool success = false;

		if (behaviorId > 0) {
			RakNet::BitStream* bs = new RakNet::BitStream((unsigned char*)startSkill.sBitStream.c_str(), startSkill.sBitStream.size(), false);

			auto* skillComponent = entity->GetComponent<SkillComponent>();

			success = skillComponent->CastPlayerSkill(behaviorId, startSkill.uiSkillHandle, bs, startSkill.optionalTargetID, startSkill.skillID);

			if (success && entity->GetCharacter()) {
				DestroyableComponent* destComp = entity->GetComponent<DestroyableComponent>();
				destComp->SetImagination(destComp->GetImagination() - skillTable->GetSkillByID(startSkill.skillID).imaginationcost);
			}

			delete bs;
		}

		if (Game::server->GetZoneID() == 1302) {
			break;
		}

		if (success) {
			//Broadcast our startSkill:
			RakNet::BitStream bitStreamLocal;
			PacketUtils::WriteHeader(bitStreamLocal, eConnectionType::CLIENT, eClientMessageType::GAME_MSG);
			bitStreamLocal.Write(entity->GetObjectID());

			EchoStartSkill echoStartSkill;
			echoStartSkill.bUsedMouse = startSkill.bUsedMouse;
			echoStartSkill.fCasterLatency = startSkill.fCasterLatency;
			echoStartSkill.iCastType = startSkill.iCastType;
			echoStartSkill.lastClickedPosit = startSkill.lastClickedPosit;
			echoStartSkill.optionalOriginatorID = startSkill.optionalOriginatorID;
			echoStartSkill.optionalTargetID = startSkill.optionalTargetID;
			echoStartSkill.originatorRot = startSkill.originatorRot;
			echoStartSkill.sBitStream = startSkill.sBitStream;
			echoStartSkill.skillID = startSkill.skillID;
			echoStartSkill.uiSkillHandle = startSkill.uiSkillHandle;
			echoStartSkill.Serialize(&bitStreamLocal);

			Game::server->Send(&bitStreamLocal, entity->GetSystemAddress(), true);
		}
	} break;

	case eGameMessageType::SYNC_SKILL: {
		RakNet::BitStream bitStreamLocal;
		PacketUtils::WriteHeader(bitStreamLocal, eConnectionType::CLIENT, eClientMessageType::GAME_MSG);
		bitStreamLocal.Write(entity->GetObjectID());
		//bitStreamLocal.Write((unsigned short)eGameMessageType::ECHO_SYNC_SKILL);
		//bitStreamLocal.Write(inStream);

		SyncSkill sync = SyncSkill(inStream); // inStream replaced &bitStream
		//sync.Serialize(&bitStreamLocal);

		ostringstream buffer;

		for (unsigned int k = 0; k < sync.sBitStream.size(); k++) {
			char s;
			s = sync.sBitStream.at(k);
			buffer << setw(2) << hex << setfill('0') << (int)s << " ";
		}

		//cout << buffer.str() << endl;

		if (usr != nullptr) {
			RakNet::BitStream* bs = new RakNet::BitStream((unsigned char*)sync.sBitStream.c_str(), sync.sBitStream.size(), false);

			auto* skillComponent = entity->GetComponent<SkillComponent>();

			skillComponent->SyncPlayerSkill(sync.uiSkillHandle, sync.uiBehaviorHandle, bs);

			delete bs;
		}

		EchoSyncSkill echo = EchoSyncSkill();
		echo.bDone = sync.bDone;
		echo.sBitStream = sync.sBitStream;
		echo.uiBehaviorHandle = sync.uiBehaviorHandle;
		echo.uiSkillHandle = sync.uiSkillHandle;

		echo.Serialize(&bitStreamLocal);

		Game::server->Send(&bitStreamLocal, sysAddr, true);
	} break;

	case eGameMessageType::REQUEST_SMASH_PLAYER:
		entity->Smash(entity->GetObjectID());
		break;

	case eGameMessageType::MOVE_ITEM_BETWEEN_INVENTORY_TYPES:
		GameMessages::HandleMoveItemBetweenInventoryTypes(inStream, entity, sysAddr);
		break;

	case eGameMessageType::MODULAR_BUILD_FINISH:
		GameMessages::HandleModularBuildFinish(inStream, entity, sysAddr);
		break;

	case eGameMessageType::PUSH_EQUIPPED_ITEMS_STATE:
		GameMessages::HandlePushEquippedItemsState(inStream, entity);
		break;

	case eGameMessageType::POP_EQUIPPED_ITEMS_STATE:
		GameMessages::HandlePopEquippedItemsState(inStream, entity);
		break;

	case eGameMessageType::BUY_FROM_VENDOR:
		GameMessages::HandleBuyFromVendor(inStream, entity, sysAddr);
		break;

	case eGameMessageType::SELL_TO_VENDOR:
		GameMessages::HandleSellToVendor(inStream, entity, sysAddr);
		break;

	case eGameMessageType::BUYBACK_FROM_VENDOR:
		GameMessages::HandleBuybackFromVendor(inStream, entity, sysAddr);
		break;

	case eGameMessageType::MODULAR_BUILD_MOVE_AND_EQUIP:
		GameMessages::HandleModularBuildMoveAndEquip(inStream, entity, sysAddr);
		break;

	case eGameMessageType::DONE_ARRANGING_WITH_ITEM:
		GameMessages::HandleDoneArrangingWithItem(inStream, entity, sysAddr);
		break;

	case eGameMessageType::MODULAR_BUILD_CONVERT_MODEL:
		GameMessages::HandleModularBuildConvertModel(inStream, entity, sysAddr);
		break;

	case eGameMessageType::BUILD_MODE_SET:
		GameMessages::HandleBuildModeSet(inStream, entity);
		break;

	case eGameMessageType::REBUILD_CANCEL:
		GameMessages::HandleRebuildCancel(inStream, entity);
		break;

	case eGameMessageType::MATCH_REQUEST:
		GameMessages::HandleMatchRequest(inStream, entity);
		break;

	case eGameMessageType::USE_NON_EQUIPMENT_ITEM:
		GameMessages::HandleUseNonEquipmentItem(inStream, entity);
		break;

	case eGameMessageType::CLIENT_ITEM_CONSUMED:
		GameMessages::HandleClientItemConsumed(inStream, entity);
		break;

	case eGameMessageType::SET_CONSUMABLE_ITEM:
		GameMessages::HandleSetConsumableItem(inStream, entity, sysAddr);
		break;

	case eGameMessageType::VERIFY_ACK:
		GameMessages::HandleVerifyAck(inStream, entity, sysAddr);
		break;

		// Trading
	case eGameMessageType::CLIENT_TRADE_REQUEST:
		GameMessages::HandleClientTradeRequest(inStream, entity, sysAddr);
		break;
	case eGameMessageType::CLIENT_TRADE_CANCEL:
		GameMessages::HandleClientTradeCancel(inStream, entity, sysAddr);
		break;
	case eGameMessageType::CLIENT_TRADE_ACCEPT:
		GameMessages::HandleClientTradeAccept(inStream, entity, sysAddr);
		break;
	case eGameMessageType::CLIENT_TRADE_UPDATE:
		GameMessages::HandleClientTradeUpdate(inStream, entity, sysAddr);
		break;

		// Pets
	case eGameMessageType::PET_TAMING_TRY_BUILD:
		GameMessages::HandlePetTamingTryBuild(inStream, entity, sysAddr);
		break;

	case eGameMessageType::NOTIFY_TAMING_BUILD_SUCCESS:
		GameMessages::HandleNotifyTamingBuildSuccess(inStream, entity, sysAddr);
		break;

	case eGameMessageType::REQUEST_SET_PET_NAME:
		GameMessages::HandleRequestSetPetName(inStream, entity, sysAddr);
		break;

	case eGameMessageType::START_SERVER_PET_MINIGAME_TIMER:
		GameMessages::HandleStartServerPetMinigameTimer(inStream, entity, sysAddr);
		break;

	case eGameMessageType::CLIENT_EXIT_TAMING_MINIGAME:
		GameMessages::HandleClientExitTamingMinigame(inStream, entity, sysAddr);
		break;

	case eGameMessageType::COMMAND_PET:
		GameMessages::HandleCommandPet(inStream, entity, sysAddr);
		break;

	case eGameMessageType::DESPAWN_PET:
		GameMessages::HandleDespawnPet(inStream, entity, sysAddr);
		break;

	case eGameMessageType::MESSAGE_BOX_RESPOND:
		GameMessages::HandleMessageBoxResponse(inStream, entity, sysAddr);
		break;

	case eGameMessageType::CHOICE_BOX_RESPOND:
		GameMessages::HandleChoiceBoxRespond(inStream, entity, sysAddr);
		break;

		// Property
	case eGameMessageType::QUERY_PROPERTY_DATA:
		GameMessages::HandleQueryPropertyData(inStream, entity, sysAddr);
		break;

	case eGameMessageType::START_BUILDING_WITH_ITEM:
		GameMessages::HandleStartBuildingWithItem(inStream, entity, sysAddr);
		break;

	case eGameMessageType::SET_BUILD_MODE:
		GameMessages::HandleSetBuildMode(inStream, entity, sysAddr);
		break;

	case eGameMessageType::PROPERTY_EDITOR_BEGIN:
		GameMessages::HandlePropertyEditorBegin(inStream, entity, sysAddr);
		break;

	case eGameMessageType::PROPERTY_EDITOR_END:
		GameMessages::HandlePropertyEditorEnd(inStream, entity, sysAddr);
		break;

	case eGameMessageType::PROPERTY_CONTENTS_FROM_CLIENT:
		GameMessages::HandlePropertyContentsFromClient(inStream, entity, sysAddr);
		break;

	case eGameMessageType::ZONE_PROPERTY_MODEL_EQUIPPED:
		GameMessages::HandlePropertyModelEquipped(inStream, entity, sysAddr);
		break;

	case eGameMessageType::PLACE_PROPERTY_MODEL:
		GameMessages::HandlePlacePropertyModel(inStream, entity, sysAddr);
		break;

	case eGameMessageType::UPDATE_MODEL_FROM_CLIENT:
		GameMessages::HandleUpdatePropertyModel(inStream, entity, sysAddr);
		break;

	case eGameMessageType::DELETE_MODEL_FROM_CLIENT:
		GameMessages::HandleDeletePropertyModel(inStream, entity, sysAddr);
		break;

	case eGameMessageType::BBB_LOAD_ITEM_REQUEST:
		GameMessages::HandleBBBLoadItemRequest(inStream, entity, sysAddr);
		break;

	case eGameMessageType::BBB_SAVE_REQUEST:
		GameMessages::HandleBBBSaveRequest(inStream, entity, sysAddr);
		break;

	case eGameMessageType::CONTROL_BEHAVIOR:
		GameMessages::HandleControlBehaviors(inStream, entity, sysAddr);
		break;

	case eGameMessageType::PROPERTY_ENTRANCE_SYNC:
		GameMessages::HandlePropertyEntranceSync(inStream, entity, sysAddr);
		break;

	case eGameMessageType::ENTER_PROPERTY1:
		GameMessages::HandleEnterProperty(inStream, entity, sysAddr);
		break;

	case eGameMessageType::ZONE_PROPERTY_MODEL_ROTATED:
		Game::entityManager->GetZoneControlEntity()->OnZonePropertyModelRotated(usr->GetLastUsedChar()->GetEntity());
		break;

	case eGameMessageType::UPDATE_PROPERTY_OR_MODEL_FOR_FILTER_CHECK:
		GameMessages::HandleUpdatePropertyOrModelForFilterCheck(inStream, entity, sysAddr);
		break;

	case eGameMessageType::SET_PROPERTY_ACCESS:
		GameMessages::HandleSetPropertyAccess(inStream, entity, sysAddr);
		break;

		// Racing
	case eGameMessageType::MODULE_ASSEMBLY_QUERY_DATA:
		GameMessages::HandleModuleAssemblyQueryData(inStream, entity, sysAddr);
		break;

	case eGameMessageType::ACKNOWLEDGE_POSSESSION:
		GameMessages::HandleAcknowledgePossession(inStream, entity, sysAddr);
		break;

	case eGameMessageType::VEHICLE_SET_WHEEL_LOCK_STATE:
		GameMessages::HandleVehicleSetWheelLockState(inStream, entity, sysAddr);
		break;

	case eGameMessageType::MODULAR_ASSEMBLY_NIF_COMPLETED:
		GameMessages::HandleModularAssemblyNIFCompleted(inStream, entity, sysAddr);
		break;

	case eGameMessageType::RACING_CLIENT_READY:
		GameMessages::HandleRacingClientReady(inStream, entity, sysAddr);
		break;

	case eGameMessageType::REQUEST_DIE:
		GameMessages::HandleRequestDie(inStream, entity, sysAddr);
		break;

	case eGameMessageType::VEHICLE_NOTIFY_SERVER_ADD_PASSIVE_BOOST_ACTION:
		GameMessages::HandleVehicleNotifyServerAddPassiveBoostAction(inStream, entity, sysAddr);
		break;

	case eGameMessageType::VEHICLE_NOTIFY_SERVER_REMOVE_PASSIVE_BOOST_ACTION:
		GameMessages::HandleVehicleNotifyServerRemovePassiveBoostAction(inStream, entity, sysAddr);
		break;

	case eGameMessageType::RACING_PLAYER_INFO_RESET_FINISHED:
		GameMessages::HandleRacingPlayerInfoResetFinished(inStream, entity, sysAddr);
		break;

	case eGameMessageType::VEHICLE_NOTIFY_HIT_IMAGINATION_SERVER:
		GameMessages::HandleVehicleNotifyHitImaginationServer(inStream, entity, sysAddr);
		break;
	case eGameMessageType::UPDATE_PROPERTY_PERFORMANCE_COST:
		GameMessages::HandleUpdatePropertyPerformanceCost(inStream, entity, sysAddr);
		break;
		// SG
	case eGameMessageType::UPDATE_SHOOTING_GALLERY_ROTATION:
		GameMessages::HandleUpdateShootingGalleryRotation(inStream, entity, sysAddr);
		break;

		// NT
	case eGameMessageType::REQUEST_MOVE_ITEM_BETWEEN_INVENTORY_TYPES:
		GameMessages::HandleRequestMoveItemBetweenInventoryTypes(inStream, entity, sysAddr);
		break;

	case eGameMessageType::TOGGLE_GHOST_REFERENCE_OVERRIDE:
		GameMessages::HandleToggleGhostReferenceOverride(inStream, entity, sysAddr);
		break;

	case eGameMessageType::SET_GHOST_REFERENCE_POSITION:
		GameMessages::HandleSetGhostReferencePosition(inStream, entity, sysAddr);
		break;

	case eGameMessageType::READY_FOR_UPDATES:
		//We don't really care about this message, as it's simply here to inform us that the client is done loading an object.
		//In the event we _do_ send an update to an object that hasn't finished loading, the client will handle it anyway.
		break;

	case eGameMessageType::REPORT_BUG:
		GameMessages::HandleReportBug(inStream, entity);
		break;

	case eGameMessageType::CLIENT_RAIL_MOVEMENT_READY:
		GameMessages::HandleClientRailMovementReady(inStream, entity, sysAddr);
		break;

	case eGameMessageType::CANCEL_RAIL_MOVEMENT:
		GameMessages::HandleCancelRailMovement(inStream, entity, sysAddr);
		break;

	case eGameMessageType::PLAYER_RAIL_ARRIVED_NOTIFICATION:
		GameMessages::HandlePlayerRailArrivedNotification(inStream, entity, sysAddr);
		break;

	case eGameMessageType::CINEMATIC_UPDATE:
		GameMessages::HandleCinematicUpdate(inStream, entity, sysAddr);
		break;

	case eGameMessageType::MODIFY_PLAYER_ZONE_STATISTIC:
		GameMessages::HandleModifyPlayerZoneStatistic(inStream, entity);
		break;

	case eGameMessageType::UPDATE_PLAYER_STATISTIC:
		GameMessages::HandleUpdatePlayerStatistic(inStream, entity);
		break;

	case eGameMessageType::DISMOUNT_COMPLETE:
		GameMessages::HandleDismountComplete(inStream, entity, sysAddr);
		break;
	case eGameMessageType::DEACTIVATE_BUBBLE_BUFF:
		GameMessages::HandleDeactivateBubbleBuff(inStream, entity);
		break;
	case eGameMessageType::ACTIVATE_BUBBLE_BUFF:
		GameMessages::HandleActivateBubbleBuff(inStream, entity);
		break;
	case eGameMessageType::ZONE_SUMMARY_DISMISSED:
		GameMessages::HandleZoneSummaryDismissed(inStream, entity);
		break;
	case eGameMessageType::REQUEST_ACTIVITY_EXIT:
		GameMessages::HandleRequestActivityExit(inStream, entity);
		break;
	case eGameMessageType::ADD_DONATION_ITEM:
		GameMessages::HandleAddDonationItem(inStream, entity, sysAddr);
		break;
	case eGameMessageType::REMOVE_DONATION_ITEM:
		GameMessages::HandleRemoveDonationItem(inStream, entity, sysAddr);
		break;
	case eGameMessageType::CONFIRM_DONATION_ON_PLAYER:
		GameMessages::HandleConfirmDonationOnPlayer(inStream, entity);
		break;
	case eGameMessageType::CANCEL_DONATION_ON_PLAYER:
		GameMessages::HandleCancelDonationOnPlayer(inStream, entity);
		break;
	default:
		Game::logger->LogDebug("GameMessageHandler", "Unknown game message ID: %i", messageID);
		break;
	}
}
