/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "GameMessageHandler.h"
#include "WorldMigration.h"
#include "MissionComponent.h"
#include "BitStreamUtils.h"
#include "dServer.h"
#include "RakNetworkFactory.h"
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
#include "CppScripts.h"

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDSkillBehaviorTable.h"
#include "SkillComponent.h"
#include "RacingControlComponent.h"
#include "ActivityMessages.h"
#include "BuildingMessages.h"
#include "RacingMessages.h"
#include "MissionMessages.h"
#include "CombatMessages.h"
#include "SkillMessages.h"
#include "TradeMessages.h"
#include "VendorMessages.h"
#include "EffectsMessages.h"
#include "InventoryMessages.h"
#include "PetMessages.h"
#include "PropertyMessages.h"
#include "MovementMessages.h"
#include "ObjectMessages.h"
#include "PlayerMessages.h"
#include "QuickBuildMessages.h"
#include "ZoneMessages.h"
#include "eMissionTaskType.h"
#include "eReplicaComponentType.h"
#include "ServiceType.h"
#include "MessageType/Game.h"
#include "ePlayerFlag.h"
#include "dConfig.h"
#include "GhostComponent.h"
#include "eGameMasterLevel.h"
#include "StringifiedEnum.h"
#include "MessageInspector.h"

namespace {
	using enum MessageType::Game;
	using namespace GameMessages;
	using MessageCreator = std::function<std::unique_ptr<GameMessages::NetGameMsg>()>;
	std::map<MessageType::Game, MessageCreator> g_MessageHandlers = {
		{ REQUEST_USE, []() { return std::make_unique<RequestUse>(); }},
		{ REQUEST_SERVER_OBJECT_INFO, []() { return std::make_unique<RequestServerObjectInfo>(); } },
		{ SHOOTING_GALLERY_FIRE, []() { return std::make_unique<ShootingGalleryFire>(); } },
		{ PICKUP_ITEM, []() { return std::make_unique<PickupItem>(); } },
		{ REQUEST_ACTIVITY_EXIT, []() { return std::make_unique<RequestActivityExit>(); } },

		// Racing
		{ MODULE_ASSEMBLY_QUERY_DATA, []() { return std::make_unique<ModuleAssemblyQueryData>(); } },
		{ VEHICLE_SET_WHEEL_LOCK_STATE, []() { return std::make_unique<VehicleSetWheelLockState>(); } },
		{ MODULAR_ASSEMBLY_NIF_COMPLETED, []() { return std::make_unique<ModularAssemblyNIFCompleted>(); } },
		{ RACING_CLIENT_READY, []() { return std::make_unique<RacingClientReady>(); } },
		{ NOTIFY_SERVER_VEHICLE_ADD_PASSIVE_BOOST_ACTION, []() { return std::make_unique<VehicleNotifyServerAddPassiveBoostAction>(); } },
		{ NOTIFY_SERVER_VEHICLE_REMOVE_PASSIVE_BOOST_ACTION, []() { return std::make_unique<VehicleNotifyServerRemovePassiveBoostAction>(); } },
		{ RACING_PLAYER_INFO_RESET_FINISHED, []() { return std::make_unique<RacingPlayerInfoResetFinished>(); } },
		{ VEHICLE_NOTIFY_HIT_IMAGINATION_SERVER, []() { return std::make_unique<VehicleNotifyHitImaginationServer>(); } },

		// Missions, flags, collectibles
		{ RESPOND_TO_MISSION, []() { return std::make_unique<RespondToMission>(); } },
		{ MISSION_DIALOGUE_OK, []() { return std::make_unique<MissionDialogueOK>(); } },
		{ REQUEST_LINKED_MISSION, []() { return std::make_unique<RequestLinkedMission>(); } },
		{ SET_FLAG, []() { return std::make_unique<SetFlag>(); } },
		{ HAS_BEEN_COLLECTED, []() { return std::make_unique<HasBeenCollected>(); } },
		{ SET_MISSION_TYPE_STATE, []() { return std::make_unique<SetMissionTypeState>(); } },

		// Effects, emotes, cinematics, UI
		{ PLAY_EMOTE, []() { return std::make_unique<PlayEmote>(); } },
		{ MESSAGE_BOX_RESPOND, []() { return std::make_unique<MessageBoxRespond>(); } },
		{ CHOICE_BOX_RESPOND, []() { return std::make_unique<ChoiceBoxRespond>(); } },
		{ CINEMATIC_UPDATE, []() { return std::make_unique<CinematicUpdate>(); } },

		// Inventory and items
		{ EQUIP_INVENTORY, []() { return std::make_unique<EquipInventory>(); } },
		{ UN_EQUIP_INVENTORY, []() { return std::make_unique<UnEquipInventory>(); } },
		{ REMOVE_ITEM_FROM_INVENTORY, []() { return std::make_unique<RemoveItemFromInventory>(); } },
		{ MOVE_ITEM_IN_INVENTORY, []() { return std::make_unique<MoveItemInInventory>(); } },
		{ MOVE_ITEM_BETWEEN_INVENTORY_TYPES, []() { return std::make_unique<MoveItemBetweenInventoryTypes>(); } },
		{ REQUEST_MOVE_ITEM_BETWEEN_INVENTORY_TYPES, []() { return std::make_unique<RequestMoveItemBetweenInventoryTypes>(); } },
		{ PUSH_EQUIPPED_ITEMS_STATE, []() { return std::make_unique<PushEquippedItemsState>(); } },
		{ POP_EQUIPPED_ITEMS_STATE, []() { return std::make_unique<PopEquippedItemsState>(); } },
		{ RESYNC_EQUIPMENT, []() { return std::make_unique<ResyncEquipment>(); } },
		{ CLIENT_ITEM_CONSUMED, []() { return std::make_unique<ClientItemConsumed>(); } },
		{ USE_NON_EQUIPMENT_ITEM, []() { return std::make_unique<UseNonEquipmentItem>(); } },
		{ SET_CONSUMABLE_ITEM, []() { return std::make_unique<SetConsumableItem>(); } },
		{ UPDATE_INVENTORY_GROUP, []() { return std::make_unique<UpdateInventoryGroup>(); } },
		{ UPDATE_INVENTORY_GROUP_CONTENTS, []() { return std::make_unique<UpdateInventoryGroupContents>(); } },

		// Pets
		{ PET_TAMING_TRY_BUILD, []() { return std::make_unique<PetTamingTryBuild>(); } },
		{ NOTIFY_TAMING_BUILD_SUCCESS, []() { return std::make_unique<NotifyTamingBuildSuccess>(); } },
		{ REQUEST_SET_PET_NAME, []() { return std::make_unique<RequestSetPetName>(); } },
		{ START_SERVER_PET_MINIGAME_TIMER, []() { return std::make_unique<StartServerPetMinigameTimer>(); } },
		{ CLIENT_EXIT_TAMING_MINIGAME, []() { return std::make_unique<ClientExitTamingMinigame>(); } },
		{ COMMAND_PET, []() { return std::make_unique<CommandPet>(); } },
		{ DESPAWN_PET, []() { return std::make_unique<DespawnPet>(); } },

		// Property
		{ SET_PROPERTY_ACCESS, []() { return std::make_unique<SetPropertyAccess>(); } },
		{ UPDATE_PROPERTY_OR_MODEL_FOR_FILTER_CHECK, []() { return std::make_unique<UpdatePropertyOrModelForFilterCheck>(); } },
		{ QUERY_PROPERTY_DATA, []() { return std::make_unique<QueryPropertyData>(); } },
		{ PROPERTY_EDITOR_BEGIN, []() { return std::make_unique<PropertyEditorBegin>(); } },
		{ PROPERTY_EDITOR_END, []() { return std::make_unique<PropertyEditorEnd>(); } },
		{ PROPERTY_CONTENTS_FROM_CLIENT, []() { return std::make_unique<PropertyContentsFromClient>(); } },
		{ ZONE_PROPERTY_MODEL_EQUIPPED, []() { return std::make_unique<ZonePropertyModelEquipped>(); } },
		{ ZONE_PROPERTY_MODEL_ROTATED, []() { return std::make_unique<ZonePropertyModelRotated>(); } },
		{ PLACE_PROPERTY_MODEL, []() { return std::make_unique<PlacePropertyModel>(); } },
		{ CREATE_MODEL_FROM_CLIENT, []() { return std::make_unique<UpdateModelFromClient>(); } },
		{ DELETE_MODEL_FROM_CLIENT, []() { return std::make_unique<DeleteModelFromClient>(); } },
		{ CONTROL_BEHAVIORS, []() { return std::make_unique<GameMessages::ControlBehaviors>(); } },
		{ PROPERTY_ENTRANCE_SYNC, []() { return std::make_unique<PropertyEntranceSync>(); } },
		{ ENTER_PROPERTY1, []() { return std::make_unique<EnterProperty1>(); } },
		{ UPDATE_PROPERTY_PERFORMANCE_COST, []() { return std::make_unique<UpdatePropertyPerformanceCost>(); } },
		{ REPORT_OFFENSIVE_MODEL, []() { return std::make_unique<ReportOffensiveModel>(); } },
		{ REPORT_OFFENSIVE_PROPERTY, []() { return std::make_unique<ReportOffensiveProperty>(); } },
		{ GET_HOT_PROPERTY_DATA, []() { return std::make_unique<GetHotPropertyData>(); } },

		// Building
		{ START_BUILDING_WITH_ITEM, []() { return std::make_unique<StartBuildingWithItem>(); } },
		{ DONE_ARRANGING_WITH_ITEM, []() { return std::make_unique<DoneArrangingWithItem>(); } },
		{ MODULAR_BUILD_FINISH, []() { return std::make_unique<ModularBuildFinish>(); } },
		{ MODULAR_BUILD_MOVE_AND_EQUIP, []() { return std::make_unique<ModularBuildMoveAndEquip>(); } },
		{ MODULAR_BUILD_CONVERT_MODEL, []() { return std::make_unique<ModularBuildConvertModel>(); } },
		{ SET_BUILD_MODE, []() { return std::make_unique<SetBuildMode>(); } },
		{ BUILD_MODE_SET, []() { return std::make_unique<BuildModeSet>(); } },
		{ UN_USE_BBB_MODEL, []() { return std::make_unique<UnUseBBBModel>(); } },
		{ FETCH_MODEL_METADATA_REQUEST, []() { return std::make_unique<FetchModelMetadataRequest>(); } },
		{ BBB_LOAD_ITEM_REQUEST, []() { return std::make_unique<BBBLoadItemRequest>(); } },
		{ BBB_SAVE_REQUEST, []() { return std::make_unique<BBBSaveRequest>(); } },
		{ SET_BBB_AUTOSAVE, []() { return std::make_unique<SetBBBAutosave>(); } },
		{ ACTIVATE_BRICK_MODE, []() { return std::make_unique<ActivateBrickMode>(); } },
		{ MOVE_INVENTORY_BATCH, []() { return std::make_unique<MoveInventoryBatch>(); } },

		// Vendors and donation vendors
		{ REQUEST_VENDOR_STATUS_UPDATE, []() { return std::make_unique<RequestVendorStatusUpdate>(); } },
		{ BUY_FROM_VENDOR, []() { return std::make_unique<BuyFromVendor>(); } },
		{ SELL_TO_VENDOR, []() { return std::make_unique<SellToVendor>(); } },
		{ BUYBACK_FROM_VENDOR, []() { return std::make_unique<BuybackFromVendor>(); } },
		{ ADD_DONATION_ITEM, []() { return std::make_unique<AddDonationItem>(); } },
		{ REMOVE_DONATION_ITEM, []() { return std::make_unique<RemoveDonationItem>(); } },
		{ CONFIRM_DONATION_ON_PLAYER, []() { return std::make_unique<ConfirmDonationOnPlayer>(); } },
		{ CANCEL_DONATION_ON_PLAYER, []() { return std::make_unique<CancelDonationOnPlayer>(); } },

		// Trading
		{ CLIENT_TRADE_REQUEST, []() { return std::make_unique<ClientTradeRequest>(); } },
		{ CLIENT_TRADE_CANCEL, []() { return std::make_unique<ClientTradeCancel>(); } },
		{ CLIENT_TRADE_ACCEPT, []() { return std::make_unique<ClientTradeAccept>(); } },
		{ CLIENT_TRADE_UPDATE, []() { return std::make_unique<ClientTradeUpdate>(); } },

		// Skills
		{ SELECT_SKILL, []() { return std::make_unique<SelectSkill>(); } },
		{ START_SKILL, []() { return std::make_unique<StartSkill>(); } },
		{ SYNC_SKILL, []() { return std::make_unique<SyncSkill>(); } },
		{ CASTER_DEAD, []() { return std::make_unique<CasterDead>(); } },
		{ REQUEST_SERVER_PROJECTILE_IMPACT, []() { return std::make_unique<RequestServerProjectileImpact>(); } },

		// Combat
		{ REQUEST_DIE, []() { return std::make_unique<RequestDie>(); } },
		{ REQUEST_SMASH_PLAYER, []() { return std::make_unique<RequestSmashPlayer>(); } },
		{ REQUEST_RESURRECT, []() { return std::make_unique<RequestResurrect>(); } },
		{ RESURRECT, []() { return std::make_unique<Resurrect>(); } },
		{ ACTIVATE_BUBBLE_BUFF, []() { return std::make_unique<ActivateBubbleBuff>(); } },
		{ DECTIVATE_BUBBLE_BUFF, []() { return std::make_unique<DeactivateBubbleBuff>(); } },
		// Zone lifecycle
		{ PLAYER_LOADED, []() { return std::make_unique<PlayerLoaded>(); } },
		{ READY_FOR_UPDATES, []() { return std::make_unique<ReadyForUpdates>(); } },
		{ NOTIFY_SERVER_LEVEL_PROCESSING_COMPLETE, []() { return std::make_unique<NotifyServerLevelProcessingComplete>(); } },
		{ ZONE_SUMMARY_DISMISSED, []() { return std::make_unique<ZoneSummaryDismissed>(); } },
		{ MISSION_DIALOGUE_CANCELLED, []() { return std::make_unique<MissionDialogueCancelled>(); } },

		// Objects and scripts
		{ FIRE_EVENT_SERVER_SIDE, []() { return std::make_unique<FireEventServerSide>(); } },

		// Player state, chat commands, reports
		{ PARSE_CHAT_MESSAGE, []() { return std::make_unique<ParseChatMessage>(); } },
		{ PICKUP_CURRENCY, []() { return std::make_unique<PickupCurrency>(); } },
		{ MODIFY_PLAYER_ZONE_STATISTIC, []() { return std::make_unique<ModifyPlayerZoneStatistic>(); } },
		{ UPDATE_PLAYER_STATISTIC, []() { return std::make_unique<UpdatePlayerStatistic>(); } },
		{ SET_TOOLTIP_FLAG, []() { return std::make_unique<SetTooltipFlag>(); } },
		{ SET_LAST_CUSTOM_BUILD, []() { return std::make_unique<SetLastCustomBuild>(); } },
		{ REPORT_BUG, []() { return std::make_unique<ReportBug>(); } },
		{ VERIFY_ACK, []() { return std::make_unique<VerifyAck>(); } },

		// Quickbuilds
		{ REBUILD_CANCEL, []() { return std::make_unique<RebuildCancel>(); } },

		// Activities, matches, leaderboards, shooting galleries
		{ MATCH_REQUEST, []() { return std::make_unique<MatchRequest>(); } },
		{ REQUEST_ACTIVITY_SUMMARY_LEADERBOARD_DATA, []() { return std::make_unique<RequestActivitySummaryLeaderboardData>(); } },
		{ SEND_ACTIVITY_SUMMARY_LEADERBOARD_DATA, []() { return std::make_unique<SendActivitySummaryLeaderboardData>(); } },
		{ ACTIVITY_STATE_CHANGE_REQUEST, []() { return std::make_unique<ActivityStateChangeRequest>(); } },
		{ UPDATE_SHOOTING_GALLERY_ROTATION, []() { return std::make_unique<UpdateShootingGalleryRotation>(); } },

		// Movement: platforms, rails, mounts, ghosting
		{ REQUEST_PLATFORM_RESYNC, []() { return std::make_unique<RequestPlatformResync>(); } },
		{ CLIENT_RAIL_MOVEMENT_READY, []() { return std::make_unique<ClientRailMovementReady>(); } },
		{ CANCEL_RAIL_MOVEMENT, []() { return std::make_unique<CancelRailMovement>(); } },
		{ PLAYER_RAIL_ARRIVED_NOTIFICATION, []() { return std::make_unique<PlayerRailArrivedNotification>(); } },
		{ REQUEST_RAIL_ACTIVATOR_STATE, []() { return std::make_unique<RequestRailActivatorState>(); } },
		{ DISMOUNT_COMPLETE, []() { return std::make_unique<DismountComplete>(); } },
		{ ACKNOWLEDGE_POSSESSION, []() { return std::make_unique<AcknowledgePossession>(); } },
		{ TOGGLE_GHOST_REFERENCE_OVERRIDE, []() { return std::make_unique<ToggleGhostReferenceOverride>(); } },
		{ SET_GHOST_REFERENCE_POSITION, []() { return std::make_unique<SetGhostReferencePosition>(); } },
	};
};

void GameMessageHandler::HandleMessage(RakNet::BitStream& inStream, const SystemAddress& sysAddr, LWOOBJID objectID, MessageType::Game messageID) {
	// The dashboard's message inspector (sees every message a client sends; nothing to do unless a capture runs)
	if (MessageInspector::IsCapturing()) MessageInspector::RecordReceived(sysAddr, objectID, messageID, inStream);

	// Get the entity
	Entity* entity = Game::entityManager->GetEntity(objectID);

	User* usr = UserManager::Instance()->GetUser(sysAddr);

	if (!usr) {
		LOG("Failed to find a logged in user for (%llu), aborting GM: %4i, %s!", objectID, messageID, StringifiedEnum::ToString(messageID).data());
		return;
	}

	if (!entity) {
		LOG("Failed to find associated entity (%llu), aborting GM: %4i, %s!", objectID, messageID, StringifiedEnum::ToString(messageID).data());
		return;
	}

	if (messageID != MessageType::Game::READY_FOR_UPDATES) LOG_DEBUG("Received GM with ID and name: %4i, %s", messageID, StringifiedEnum::ToString(messageID).data());

	auto handler = g_MessageHandlers.find(messageID);
	if (handler != g_MessageHandlers.end()) {
		auto msg = handler->second();

		// Verify that the system address user is able to use this message.
		if (msg->requiredGmLevel > eGameMasterLevel::CIVILIAN) {
			auto* usingEntity = Game::entityManager->GetEntity(usr->GetLoggedInChar());
			if (!usingEntity || usingEntity->GetGMLevel() < msg->requiredGmLevel) {
				if (usingEntity) LOG("User %s (%llu) does not have the required GM level to execute this command.", usingEntity->GetCharacter()->GetName().c_str(), usingEntity->GetObjectID());
				else LOG("ObjectID %llu tried to use a gm required message.", usr->GetLoggedInChar());
				return;
			}
		}

		if (!msg->Deserialize(inStream)) {
			LOG("Dropping malformed GM %4i, %s from (%llu) targeting (%llu)", messageID, StringifiedEnum::ToString(messageID).data(), usr->GetLoggedInChar(), objectID);
			return;
		}
		msg->Handle(*entity, sysAddr);
		return;
	}

	LOG_DEBUG("Received Unknown GM with ID: %4i, %s", messageID, StringifiedEnum::ToString(messageID).data());
}
