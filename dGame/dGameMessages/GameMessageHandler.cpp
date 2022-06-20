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

#include "Client/PlayEmote.h"
#include "Client/MoveItemInInventory.h"
#include "Client/RemoveItemFromInventory.h"
#include "Client/EquipItem.h"
#include "Client/RespondToMission.h"
#include "Client/RequestUse.h"
#include "Client/SetFlag.h"
#include "Client/HasBeenCollected.h"
#include "Client/PlayerLoaded.h"
#include "Client/RequestLinkedMission.h"
#include "Client/MissionDialogOK.h"
#include "Client/RequestPlatformResync.h"
#include "Client/FireEventServerSide.h"
#include "Client/RequestActivitySummaryLeaderboardData.h"
#include "Client/ActivityStateChangeRequest.h"
#include "Client/ParseChatMessage.h"
#include "Client/NotifyServerLevelProcessingComplete.h"
#include "Client/PickupCurrency.h"
#include "Client/Resurrect.h"
#include "Client/RequestResurrect.h"
#include "Client/RequestSmashPlayer.h"
#include "Client/MoveItemBetweenInventoryTypes.h"

#include "Dual/StartSkill.h"
#include "Dual/RequestServerProjectileImpact.h"
#include "Dual/SyncSkill.h"

using namespace std;

void GameMessageHandler::HandleMessage(RakNet::BitStream* inStream, const SystemAddress& sysAddr, LWOOBJID objectID, GAME_MSG messageID) {

	CBITSTREAM

		// Get the entity
		Entity* entity = EntityManager::Instance()->GetEntity(objectID);

	User* usr = UserManager::Instance()->GetUser(sysAddr);

	GameMessage* returnMessage = nullptr;

	if (!entity)
	{
		Game::logger->Log("GameMessageHandler", "Failed to find associated entity (%llu), aborting GM (%X)!\n", objectID, messageID);
		return;
	}

	switch (messageID) {
	case GAME_MSG_PLAY_EMOTE:
		returnMessage = new PlayEmote();
		break;

	case GAME_MSG_MOVE_ITEM_IN_INVENTORY:
		returnMessage = new MoveItemInInventory();
		break;

	case GAME_MSG_REMOVE_ITEM_FROM_INVENTORY:
		returnMessage = new RemoveItemFromInventory();
		break;

	case GAME_MSG_EQUIP_ITEM:
		returnMessage = new EquipItem();
		break;

	case GAME_MSG_UN_EQUIP_ITEM:
		returnMessage = new UnEquipItem();
		break;

	case GAME_MSG_RESPOND_TO_MISSION:
		returnMessage = new RespondToMission();
		break;

	case GAME_MSG_REQUEST_USE:
		returnMessage = new RequestUse();
		break;

	case GAME_MSG_SET_FLAG:
		returnMessage = new SetFlag();
		break;

	case GAME_MSG_HAS_BEEN_COLLECTED:
		returnMessage = new HasBeenCollected();
		break; // contains handler

	case GAME_MSG_PLAYER_LOADED:
		returnMessage = new PlayerLoaded();
		break;

	case GAME_MSG_REQUEST_LINKED_MISSION:
		returnMessage = new RequestLinkedMission();
		break;

	case GAME_MSG_MISSION_DIALOGUE_OK:
		returnMessage = new MissionDialogOK();
		break;

	case GAME_MSG_REQUEST_PLATFORM_RESYNC:
		returnMessage = new RequestPlatformResync();
		break; // contains handler

	case GAME_MSG_FIRE_EVENT_SERVER_SIDE:
		returnMessage = new FireEventServerSide();
		break; // contains handler

	case GAME_MSG_REQUEST_ACTIVITY_SUMMARY_LEADERBOARD_DATA:
		returnMessage = new RequestActivitySummaryLeaderboardData();
		break;

	case GAME_MSG_ACTIVITY_STATE_CHANGE_REQUEST:
		returnMessage = new ActivityStateChangeRequest();
		break;

	case GAME_MSG_PARSE_CHAT_MESSAGE:
		returnMessage = new ParseChatMessage();
		break;

	case GAME_MSG_NOTIFY_SERVER_LEVEL_PROCESSING_COMPLETE:
		returnMessage = new NotifyServerLevelProcessingComplete();
		break;

	case GAME_MSG_PICKUP_CURRENCY:
		returnMessage = new PickupCurrency();
		break;

	case GAME_MSG_PICKUP_ITEM:
		returnMessage = new PickupItem();
		break;

	case GAME_MSG_RESURRECT:
		returnMessage = new Resurrect();
		break;

	case GAME_MSG_REQUEST_RESURRECT:
		returnMessage = new RequestResurrect();
		break; // contains handler

	case GAME_MSG_REQUEST_SERVER_PROJECTILE_IMPACT:
		returnMessage = new RequestServerProjectileImpact();
		break;
	case GAME_MSG_START_SKILL:
		returnMessage = new StartSkill();
		break; // contains handler

	case GAME_MSG_SYNC_SKILL:
		returnMessage = new SyncSkill();
		break;

	case GAME_MSG_REQUEST_SMASH_PLAYER:
		returnMessage = new RequestSmashPlayer();
		break;

	case GAME_MSG_MOVE_ITEM_BETWEEN_INVENTORY_TYPES:
		returnMessage = new MoveItemBetweenInventoryTypes();
		break;

	case GAME_MSG_MODULAR_BUILD_FINISH:
		GameMessages::HandleModularBuildFinish(inStream, entity, sysAddr);
		break;

	case GAME_MSG_PUSH_EQUIPPED_ITEMS_STATE:
		GameMessages::HandlePushEquippedItemsState(inStream, entity);
		break;

	case GAME_MSG_POP_EQUIPPED_ITEMS_STATE:
		GameMessages::HandlePopEquippedItemsState(inStream, entity);
		break;

	case GAME_MSG_BUY_FROM_VENDOR:
		GameMessages::HandleBuyFromVendor(inStream, entity, sysAddr);
		break;

	case GAME_MSG_SELL_TO_VENDOR:
		GameMessages::HandleSellToVendor(inStream, entity, sysAddr);
		break;

	case GAME_MSG_BUYBACK_FROM_VENDOR:
		GameMessages::HandleBuybackFromVendor(inStream, entity, sysAddr);
		break;

	case GAME_MSG_MODULAR_BUILD_MOVE_AND_EQUIP:
		GameMessages::HandleModularBuildMoveAndEquip(inStream, entity, sysAddr);
		break;

	case GAME_MSG_DONE_ARRANGING_WITH_ITEM:
		GameMessages::HandleDoneArrangingWithItem(inStream, entity, sysAddr);
		break;

	case GAME_MSG_MODULAR_BUILD_CONVERT_MODEL:
		GameMessages::HandleModularBuildConvertModel(inStream, entity, sysAddr);
		break;

	case GAME_MSG_BUILD_MODE_SET:
		GameMessages::HandleBuildModeSet(inStream, entity);
		break;

	case GAME_MSG_REBUILD_CANCEL:
		GameMessages::HandleRebuildCancel(inStream, entity);
		break;

	case GAME_MSG_MATCH_REQUEST:
		GameMessages::HandleMatchRequest(inStream, entity);
		break;

	case GAME_MSG_USE_NON_EQUIPMENT_ITEM:
		GameMessages::HandleUseNonEquipmentItem(inStream, entity);
		break;

	case GAME_MSG_CLIENT_ITEM_CONSUMED:
		GameMessages::HandleClientItemConsumed(inStream, entity);
		break;

	case GAME_MSG_SET_CONSUMABLE_ITEM:
		GameMessages::HandleSetConsumableItem(inStream, entity, sysAddr);
		break;

	case GAME_MSG_VERIFY_ACK:
		GameMessages::HandleVerifyAck(inStream, entity, sysAddr);
		break;

		// Trading
	case GAME_MSG_CLIENT_TRADE_REQUEST:
		GameMessages::HandleClientTradeRequest(inStream, entity, sysAddr);
		break;
	case GAME_MSG_CLIENT_TRADE_CANCEL:
		GameMessages::HandleClientTradeCancel(inStream, entity, sysAddr);
		break;
	case GAME_MSG_CLIENT_TRADE_ACCEPT:
		GameMessages::HandleClientTradeAccept(inStream, entity, sysAddr);
		break;
	case GAME_MSG_CLIENT_TRADE_UPDATE:
		GameMessages::HandleClientTradeUpdate(inStream, entity, sysAddr);
		break;

		// Pets
	case GAME_MSG_PET_TAMING_TRY_BUILD:
		GameMessages::HandlePetTamingTryBuild(inStream, entity, sysAddr);
		break;

	case GAME_MSG_NOTIFY_TAMING_BUILD_SUCCESS:
		GameMessages::HandleNotifyTamingBuildSuccess(inStream, entity, sysAddr);
		break;

	case GAME_MSG_REQUEST_SET_PET_NAME:
		GameMessages::HandleRequestSetPetName(inStream, entity, sysAddr);
		break;

	case GAME_MSG_START_SERVER_PET_MINIGAME_TIMER:
		GameMessages::HandleStartServerPetMinigameTimer(inStream, entity, sysAddr);
		break;

	case GAME_MSG_CLIENT_EXIT_TAMING_MINIGAME:
		GameMessages::HandleClientExitTamingMinigame(inStream, entity, sysAddr);
		break;

	case GAME_MSG_COMMAND_PET:
		GameMessages::HandleCommandPet(inStream, entity, sysAddr);
		break;

	case GAME_MSG_DESPAWN_PET:
		GameMessages::HandleDespawnPet(inStream, entity, sysAddr);
		break;

	case GAME_MSG_MESSAGE_BOX_RESPOND:
		GameMessages::HandleMessageBoxResponse(inStream, entity, sysAddr);
		break;

	case GAME_MSG_CHOICE_BOX_RESPOND:
		GameMessages::HandleChoiceBoxRespond(inStream, entity, sysAddr);
		break;

		// Property
	case GAME_MSG_QUERY_PROPERTY_DATA:
		GameMessages::HandleQueryPropertyData(inStream, entity, sysAddr);
		break;

	case GAME_MSG_START_BUILDING_WITH_ITEM:
		GameMessages::HandleStartBuildingWithItem(inStream, entity, sysAddr);
		break;

	case GAME_MSG_SET_BUILD_MODE:
		GameMessages::HandleSetBuildMode(inStream, entity, sysAddr);
		break;

	case GAME_MSG_PROPERTY_EDITOR_BEGIN:
		GameMessages::HandlePropertyEditorBegin(inStream, entity, sysAddr);
		break;

	case GAME_MSG_PROPERTY_EDITOR_END:
		GameMessages::HandlePropertyEditorEnd(inStream, entity, sysAddr);
		break;

	case GAME_MSG_PROPERTY_CONTENTS_FROM_CLIENT:
		GameMessages::HandlePropertyContentsFromClient(inStream, entity, sysAddr);
		break;

	case GAME_MSG_ZONE_PROPERTY_MODEL_EQUIPPED:
		GameMessages::HandlePropertyModelEquipped(inStream, entity, sysAddr);
		break;

	case GAME_MSG_PLACE_PROPERTY_MODEL:
		GameMessages::HandlePlacePropertyModel(inStream, entity, sysAddr);
		break;

	case GAME_MSG_UPDATE_MODEL_FROM_CLIENT:
		GameMessages::HandleUpdatePropertyModel(inStream, entity, sysAddr);
		break;

	case GAME_MSG_DELETE_MODEL_FROM_CLIENT:
		GameMessages::HandleDeletePropertyModel(inStream, entity, sysAddr);
		break;

	case GAME_MSG_BBB_LOAD_ITEM_REQUEST:
		GameMessages::HandleBBBLoadItemRequest(inStream, entity, sysAddr);
		break;

	case GAME_MSG_BBB_SAVE_REQUEST:
		GameMessages::HandleBBBSaveRequest(inStream, entity, sysAddr);
		break;

	case GAME_MSG_PROPERTY_ENTRANCE_SYNC:
		GameMessages::HandlePropertyEntranceSync(inStream, entity, sysAddr);
		break;

	case GAME_MSG_ENTER_PROPERTY1:
		GameMessages::HandleEnterProperty(inStream, entity, sysAddr);
		break;

	case GAME_MSG_ZONE_PROPERTY_MODEL_ROTATED:
		EntityManager::Instance()->GetZoneControlEntity()->OnZonePropertyModelRotated(usr->GetLastUsedChar()->GetEntity());
		break;

	case GAME_MSG_UPDATE_PROPERTY_OR_MODEL_FOR_FILTER_CHECK:
		GameMessages::HandleUpdatePropertyOrModelForFilterCheck(inStream, entity, sysAddr);
		break;

	case GAME_MSG_SET_PROPERTY_ACCESS:
		GameMessages::HandleSetPropertyAccess(inStream, entity, sysAddr);
		break;

		// Racing
	case GAME_MSG_MODULE_ASSEMBLY_QUERY_DATA:
		GameMessages::HandleModuleAssemblyQueryData(inStream, entity, sysAddr);
		break;

	case GAME_MSG_ACKNOWLEDGE_POSSESSION:
		GameMessages::HandleAcknowledgePossession(inStream, entity, sysAddr);
		break;

	case GAME_MSG_VEHICLE_SET_WHEEL_LOCK_STATE:
		GameMessages::HandleVehicleSetWheelLockState(inStream, entity, sysAddr);
		break;

	case GAME_MSG_MODULAR_ASSEMBLY_NIF_COMPLETED:
		GameMessages::HandleModularAssemblyNIFCompleted(inStream, entity, sysAddr);
		break;

	case GAME_MSG_RACING_CLIENT_READY:
		GameMessages::HandleRacingClientReady(inStream, entity, sysAddr);
		break;

	case GAME_MSG_REQUEST_DIE:
		GameMessages::HandleRequestDie(inStream, entity, sysAddr);
		break;

	case GAME_MSG_VEHICLE_NOTIFY_SERVER_ADD_PASSIVE_BOOST_ACTION:
		GameMessages::HandleVehicleNotifyServerAddPassiveBoostAction(inStream, entity, sysAddr);
		break;

	case GAME_MSG_VEHICLE_NOTIFY_SERVER_REMOVE_PASSIVE_BOOST_ACTION:
		GameMessages::HandleVehicleNotifyServerRemovePassiveBoostAction(inStream, entity, sysAddr);
		break;

	case GAME_MSG_RACING_PLAYER_INFO_RESET_FINISHED:
		GameMessages::HandleRacingPlayerInfoResetFinished(inStream, entity, sysAddr);
		break;

	case GAME_MSG_VEHICLE_NOTIFY_HIT_IMAGINATION_SERVER:
		GameMessages::HandleVehicleNotifyHitImaginationServer(inStream, entity, sysAddr);
		break;
	case GAME_MSG_UPDATE_PROPERTY_PERFORMANCE_COST:
		GameMessages::HandleUpdatePropertyPerformanceCost(inStream, entity, sysAddr);
		break;
		// SG
	case GAME_MSG_UPDATE_SHOOTING_GALLERY_ROTATION:
		GameMessages::HandleUpdateShootingGalleryRotation(inStream, entity, sysAddr);
		break;

		// NT
	case GAME_MSG_REQUEST_MOVE_ITEM_BETWEEN_INVENTORY_TYPES:
		GameMessages::HandleRequestMoveItemBetweenInventoryTypes(inStream, entity, sysAddr);
		break;

	case GAME_MSG_TOGGLE_GHOST_REFERENCE_OVERRIDE:
		GameMessages::HandleToggleGhostReferenceOverride(inStream, entity, sysAddr);
		break;

	case GAME_MSG_SET_GHOST_REFERENCE_POSITION:
		GameMessages::HandleSetGhostReferencePosition(inStream, entity, sysAddr);
		break;

	case GAME_MSG_READY_FOR_UPDATES:
		//We don't really care about this message, as it's simply here to inform us that the client is done loading an object.
		//In the event we _do_ send an update to an object that hasn't finished loading, the client will handle it anyway.
		break;

	case GAME_MSG_REPORT_BUG:
		GameMessages::HandleReportBug(inStream, entity);
		break;

	case GAME_MSG_CLIENT_RAIL_MOVEMENT_READY:
		GameMessages::HandleClientRailMovementReady(inStream, entity, sysAddr);
		break;

	case GAME_MSG_CANCEL_RAIL_MOVEMENT:
		GameMessages::HandleCancelRailMovement(inStream, entity, sysAddr);
		break;

	case GAME_MSG_PLAYER_RAIL_ARRIVED_NOTIFICATION:
		GameMessages::HandlePlayerRailArrivedNotification(inStream, entity, sysAddr);
		break;

	case GAME_MSG_CINEMATIC_UPDATE:
		GameMessages::HandleCinematicUpdate(inStream, entity, sysAddr);
		break;

	case GAME_MSG_MODIFY_PLAYER_ZONE_STATISTIC:
		GameMessages::HandleModifyPlayerZoneStatistic(inStream, entity);
		break;

	case GAME_MSG_UPDATE_PLAYER_STATISTIC:
		GameMessages::HandleUpdatePlayerStatistic(inStream, entity);
		break;

	default:
		//Game::logger->Log("GameMessageHandler", "Unknown game message ID: %X\n", messageID);
		break;
	}

	if (returnMessage) {
		returnMessage->Deserialize(inStream);
		returnMessage->associate = entity;
		returnMessage->sysAddr = sysAddr;
		returnMessage->Handle(); // This is for GMs which I am unsure of their target components

		for (const auto& pair : entity->GetComponents()) {
			auto* comp = pair.second;

			for (const auto& handlerPair : comp->GetHandlers()) {
				if (handlerPair.first == messageID) {
					(comp->*(handlerPair.second))(returnMessage);
				}
			}
		}

		delete returnMessage;
	}
}
