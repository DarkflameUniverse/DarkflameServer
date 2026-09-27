#include "BrickByBrick.h"

#include "BuildingMessages.h"
#include "CDBrickIDTableTable.h"
#include "CDClientManager.h"
#include "Character.h"
#include "ClientPackets.h"
#include "DashboardNotify.h"
#include "Database.h"
#include "dConfig.h"
#include "EconomyLedger.h"
#include "eBlueprintSaveResponseType.h"
#include "eInventoryType.h"
#include "eLootSourceType.h"
#include "Entity.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Inventory.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"
#include "Item.h"
#include "Logger.h"
#include "Lxfml.h"
#include "ObjectIDManager.h"
#include "PropertyManagementComponent.h"
#include "PropertyMessages.h"
#include "Sd0.h"
#include "UgcKeys.h"
#include "User.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <ranges>
#include <sstream>
#include <unordered_map>

namespace {
	// Every item in one inventory, copied so the inventory can change while going through them
	std::vector<Item*> ItemsIn(InventoryComponent& inventory, const eInventoryType type) {
		std::vector<Item*> items;
		auto* bag = inventory.GetInventory(type);
		if (!bag) return items;
		for (auto* item : bag->GetItems() | std::views::values) items.push_back(item);
		return items;
	}

	std::vector<LWOOBJID> IdsIn(InventoryComponent& inventory, const eInventoryType type) {
		std::vector<LWOOBJID> ids;
		for (const auto* item : ItemsIn(inventory, type)) ids.push_back(item->GetId());
		return ids;
	}

	// Moves one unique item (a model) to another of the player's inventories keeping its object id, as a live server
	// did for BBBLoadItemRequest: RemoveItemFromInventory for the old place, AddItemToInventoryClientSync for the new.
	LWOOBJID MoveKeepingId(InventoryComponent& inventory, Item* item, const eInventoryType to) {
		if (!item) return LWOOBJID_EMPTY;
		const auto from = item->GetInventory()->GetType();
		if (from == to) return item->GetId();

		const auto id = item->GetId();
		const auto lot = item->GetLot();
		const auto count = item->GetCount();
		const auto subKey = item->GetSubKey();
		const auto bound = item->GetBound();
		const LwoNameValue config = item->GetConfig();

		EconomyLedger::ScopedItemTransfer moving;
		if (item->IsEquipped()) item->UnEquip();
		item->SetCount(0, false, false, false, eLootSourceType::RELOCATE);
		const auto received = inventory.ReceiveItem(id, lot, count, eLootSourceType::RELOCATE, config, subKey, bound,
			{ .inventory = to, .showFlyingLoot = false, .sourceInventory = from });
		return received.id;
	}

	// Bricks the player took into brick by brick building: used up by the model (as live did) when bbb_consume_bricks
	// is 1, otherwise back to the backpack (DLU's choice: building does not cost bricks).
	void SettleBricks(InventoryComponent& inventory, const bool usedByModel) {
		const bool consume = usedByModel && Game::config && Game::config->GetValue("bbb_consume_bricks") == "1";
		for (auto* item : ItemsIn(inventory, eInventoryType::BRICKS_IN_BBB)) {
			if (consume) {
				item->SetCount(0, false, false, false, eLootSourceType::INVENTORY);
			} else {
				inventory.MoveItemToInventory(item, eInventoryType::BRICKS, item->GetCount(), false);
			}
		}
	}

	struct SavedModel {
		LWOOBJID modelId{};
		LWOOBJID blueprintId{};
		NiPoint3 center{};
		std::string sd0{}; // the model's own sd0 compressed LXFML
	};

	// Splits a build into its models and stores each as a new ugc row (is_optimized 0: the UGC server makes its mesh
	// and icon). Every save makes new blueprint ids: the client caches blueprints by id.
	std::vector<SavedModel> StoreModels(Entity& player, const std::string& sd0Data, const std::string& debugName) {
		std::vector<SavedModel> saved;
		auto* character = player.GetCharacter();
		if (!character || !character->GetParentUser()) return saved;

		std::istringstream sd0Stream(sd0Data);
		Sd0 sd0(sd0Stream);
		const auto lxfml = sd0.GetAsStringUncompressed();
		if (lxfml.empty()) return saved;

		if (Game::config && Game::config->GetValue("save_lxfmls") == "1") {
			std::ofstream outFile("debug_lxfml_uncompressed_" + debugName + ".lxfml");
			outFile << lxfml;
		}

		// A quiet period before the UGC server makes the models (ugc_debounce_seconds, sharedconfig.ini): a model
		// the owner keeps editing isn't made for every save (docs/UgcServer.md)
		const auto debounce = Game::config ? GeneralUtils::TryParse<int64_t>(Game::config->GetValue("ugc_debounce_seconds")).value_or(120) : 120;
		const auto processAfter = UgcDebounce::ProcessAfter(std::time(nullptr), debounce);
		for (const auto& part : Lxfml::Split(lxfml)) {
			const auto [modelId, blueprintId] = ObjectIDManager::GetNewModelIDs();
			Sd0 model = sd0;
			model.FromData(reinterpret_cast<const uint8_t*>(part.lxfml.data()), part.lxfml.size());
			auto stream = model.GetAsStream();
			Database::Get()->InsertNewUgcModel(stream, blueprintId, character->GetParentUser()->GetAccountID(), character->GetID(), processAfter);

			auto& entry = saved.emplace_back();
			entry.modelId = modelId;
			entry.blueprintId = blueprintId;
			entry.center = part.center;
			for (const auto& chunk : model.GetAsVector()) entry.sd0.append(reinterpret_cast<const char*>(chunk.data()), chunk.size());
		}
		return saved;
	}

	// A config value as text (empty when missing)
	std::string ConfigText(const LwoNameValue& config, const std::u16string& key) {
		const auto it = config.find(key);
		if (it == config.end() || !it->second) return "";
		return it->second->GetValueAsString();
	}

	// A brick's LOT by its LEGO design id (BrickIDTable); 0 when the design has no item
	LOT BrickLot(const uint32_t designId) {
		static std::unordered_map<uint32_t, LOT> lots;
		if (lots.empty()) {
			for (const auto& entry : CDClientManager::GetTable<CDBrickIDTableTable>()->Query([](const CDBrickIDTable&) { return true; })) {
				lots.try_emplace(entry.LEGOBrickID, static_cast<LOT>(entry.NDObjectID));
			}
		}
		const auto it = lots.find(designId);
		return it == lots.end() ? 0 : it->second;
	}

	// The blueprint data of a stored brick built model (its ugc row); false when there is no such row
	bool FillBlueprintMetadata(const LWOOBJID blueprintId, GameMessages::BlueprintMetadata& data) {
		auto model = Database::Get()->GetUgcModel(blueprintId);
		if (!model) return false;

		data.blueprintID = blueprintId;
		Sd0 sd0(model->lxfmlData);
		const auto contents = Lxfml::ReadContents(sd0.GetAsStringUncompressed());
		std::vector<LOT> lots;
		for (const auto designId : contents.designIds) {
			const auto lot = BrickLot(designId);
			if (lot != 0) lots.push_back(lot);
		}
		data.brickListColonDelim = BrickByBrick::BrickList(lots);
		data.numberOfBricks = static_cast<int32_t>(contents.designIds.size());
		data.modelBoxMins = contents.boxMin;
		data.modelBoxMaxs = contents.boxMax;
		return true;
	}
}

bool BrickByBrick::IsEmptyModel(const std::string_view sd0) {
	// The client clears its autosave with the bare sd0 header ("sd0" 01 ff) and no chunks
	return sd0.size() <= 5;
}

BrickByBrick::ModelRemoval BrickByBrick::PlanModelRemoval(const int32_t reason) {
	switch (static_cast<eDeleteReason>(reason)) {
	case eDeleteReason::PICKING_MODEL_UP: return { .equip = true, .notifyPostDelete = true };
	case eDeleteReason::BREAKING_MODEL_APART: return { .equip = false, .notifyPostDelete = true };
	default: return { .equip = false, .notifyPostDelete = false };
	}
}

BrickByBrick::Recovery BrickByBrick::PlanRecovery(const std::vector<LWOOBJID>& modelsInBbb, const std::optional<IBbbAutosave::Info>& autosave) {
	Recovery recovery;
	recovery.rebuild = autosave.has_value() && !IsEmptyModel(autosave->lxfml);
	if (recovery.rebuild) recovery.consume = autosave->sourceItems;
	for (const auto id : modelsInBbb) {
		if (std::ranges::find(recovery.consume, id) == recovery.consume.end()) recovery.giveBack.push_back(id);
	}
	return recovery;
}

LwoNameValue BrickByBrick::ModelItemConfig(const LWOOBJID blueprintId, const LWOOBJID userModelId, const std::string& behaviors) {
	// The keys a live server gave a brick built model item (AddItemToInventoryClientSync when picking one up)
	LwoNameValue config;
	config.Insert<LWOOBJID>(u"blueprintid", blueprintId);
	config.Insert<LWOOBJID>(u"userModelID", userModelId);
	config.Insert<std::u16string>(u"userModelName", u"");
	config.Insert<std::u16string>(u"userModelDesc", u"");
	config.Insert<bool>(u"userModelHasBhvr", !behaviors.empty() && behaviors.find_first_not_of("0,") != std::string::npos);
	config.Insert<std::string>(u"userModelBehaviors", behaviors.empty() ? "0,0,0,0,0" : behaviors);
	config.Insert<std::string>(u"userModelBehaviorSourceIDs", "0,0,0,0,0");
	config.Insert<bool>(u"userModelOpt", true);
	config.Insert<int32_t>(u"userModelMod", 1);
	config.Insert<int32_t>(u"userModelPhysicsType", 2);
	return config;
}

LWOOBJID BrickByBrick::ConfigObjectId(const LwoNameValue& config, const std::u16string& key) {
	const auto it = config.find(key);
	if (it == config.end() || !it->second) return LWOOBJID_EMPTY;
	return GeneralUtils::TryParse<LWOOBJID>(it->second->GetValueAsString()).value_or(LWOOBJID_EMPTY);
}

uint32_t BrickByBrick::InventoryToLoadInto(const uint32_t savedType) {
	if (savedType == eInventoryType::MODELS_IN_BBB) return eInventoryType::MODELS;
	if (savedType == eInventoryType::BRICKS_IN_BBB) return eInventoryType::BRICKS;
	return savedType;
}

LWOOBJID BrickByBrick::LoadModel(Entity& player, const LWOOBJID itemId) {
	auto* inventory = player.GetComponent<InventoryComponent>();
	if (!inventory) return LWOOBJID_EMPTY;

	auto* item = inventory->FindItemById(itemId);
	if (!item) {
		LOG("Player %llu opened model item %llu in brick by brick, but has no such item", player.GetObjectID(), itemId);
		return LWOOBJID_EMPTY;
	}

	const auto type = item->GetInventory()->GetType();
	if (type == eInventoryType::MODELS_IN_BBB) return itemId;
	if (type != eInventoryType::MODELS) {
		LOG("Player %llu opened item %llu from inventory %i in brick by brick", player.GetObjectID(), itemId, type);
		return LWOOBJID_EMPTY;
	}

	return MoveKeepingId(*inventory, item, eInventoryType::MODELS_IN_BBB);
}

void BrickByBrick::Save(Entity& player, const LWOOBJID localId, const std::string& sd0Data) {
	ClientPackets::BlueprintSaveResponse response;
	response.localId = localId;

	auto* inventory = player.GetComponent<InventoryComponent>();
	auto* property = PropertyManagementComponent::Instance();
	// Brick by brick building happens on the player's own property; the build becomes models placed there
	if (!inventory || !property || property->GetOwnerId() != player.GetObjectID()) {
		LOG("Player %llu saved a brick by brick model but is not on their own property", player.GetObjectID());
		response.reasonCode = eBlueprintSaveResponseType::PlacementFailed;
		response.Send(player.GetSystemAddress());
		return;
	}

	const auto models = StoreModels(player, sd0Data, std::to_string(localId));
	if (models.empty()) {
		// BBB_COULD_NOT_GENERATE_MODEL; the client keeps the build
		response.reasonCode = eBlueprintSaveResponseType::ModelGenerationFailed;
		response.Send(player.GetSystemAddress());
		return;
	}
	LOG_DEBUG("Split into %zu models", models.size());

	// As live: the bricks are settled, then the response, then the models that were opened are gone
	SettleBricks(*inventory, true);

	response.reasonCode = eBlueprintSaveResponseType::EverythingWorked;
	for (const auto& model : models) {
		auto& responseModel = response.models.emplace_back();
		responseModel.blueprintId = model.blueprintId;
		responseModel.data = model.sd0;
	}
	response.Send(player.GetSystemAddress());

	for (auto* item : ItemsIn(*inventory, eInventoryType::MODELS_IN_BBB)) {
		item->SetCount(0, false, false, false, eLootSourceType::INVENTORY);
	}
	Database::Get()->DeleteBbbAutosave(player.GetObjectID());

	for (const auto& model : models) {
		LwoNameValue config;
		config.Insert<LWOOBJID>(u"blueprintid", model.blueprintId);
		property->SpawnModel(MODEL_OBJECT_LOT, model.modelId, model.center, QuatUtils::IDENTITY, config);
	}
	property->Save();
	DashboardNotify::Changed("properties", property->GetId());

	GameMessages::RequeryPropertyModels requery;
	requery.target = player.GetObjectID();
	requery.SendToClient(player.GetSystemAddress());
}

void BrickByBrick::Autosave(Entity& player, const std::string& sd0) {
	if (IsEmptyModel(sd0)) {
		Database::Get()->DeleteBbbAutosave(player.GetObjectID());
		return;
	}

	auto* inventory = player.GetComponent<InventoryComponent>();
	IBbbAutosave::Info info;
	info.lxfml = sd0;
	if (inventory) info.sourceItems = IdsIn(*inventory, eInventoryType::MODELS_IN_BBB);
	info.updatedAt = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	Database::Get()->SetBbbAutosave(player.GetObjectID(), info);
}

void BrickByBrick::ReturnModel(Entity& player, const LWOOBJID itemId, const bool hasWorldTransform, const NiPoint3& position, const NiQuaternion& rotation) {
	auto* inventory = player.GetComponent<InventoryComponent>();
	if (inventory) {
		auto* item = inventory->FindItemById(itemId);
		if (item && item->GetInventory()->GetType() == eInventoryType::MODELS_IN_BBB) {
			auto* property = PropertyManagementComponent::Instance();
			if (hasWorldTransform && property && property->GetOwnerId() == player.GetObjectID()) {
				property->PlaceModelFromItem(*item, position, rotation);
				property->SendModelsOnProperty();
			} else {
				MoveKeepingId(*inventory, item, eInventoryType::MODELS);
			}
		} else {
			LOG("Player %llu returned model item %llu, which is not in their brick by brick inventory", player.GetObjectID(), itemId);
		}
	}

	// The autosave must not use up a model the player has back
	auto autosave = Database::Get()->GetBbbAutosave(player.GetObjectID());
	if (autosave && std::erase(autosave->sourceItems, itemId) != 0) Database::Get()->SetBbbAutosave(player.GetObjectID(), *autosave);

	if (hasWorldTransform) {
		// The client waits in its saving state after sending this (BBBManager::SendUnUseBBBModel, 0x00b6b210); any
		// response with the local id it expects (none pending: 0) ends it. A code other than EverythingWorked keeps
		// the build the player is working on (EverythingWorked would clear it and leave brick mode).
		ClientPackets::BlueprintSaveResponse response;
		response.localId = LWOOBJID_EMPTY;
		response.reasonCode = eBlueprintSaveResponseType::PlacementFailed;
		response.Send(player.GetSystemAddress());
	}
}

void BrickByBrick::MoveBricks(Entity& player, const GameMessages::MoveInventoryBatch& move) {
	const auto isBrickBag = [](const eInventoryType type) { return type == eInventoryType::BRICKS || type == eInventoryType::BRICKS_IN_BBB; };
	if (!isBrickBag(move.srcBag) || !isBrickBag(move.dstBag) || move.srcBag == move.dstBag || move.moveLOT == LOT_NULL || move.count == 0) {
		LOG_DEBUG("Ignoring MoveInventoryBatch from %i to %i of %i", move.srcBag, move.dstBag, move.moveLOT);
		return;
	}

	auto* inventory = player.GetComponent<InventoryComponent>();
	if (!inventory) return;
	auto* source = inventory->GetInventory(move.srcBag);
	if (!source) return;

	const auto available = source->GetLotCount(move.moveLOT);
	const auto toMove = std::min(move.count, available);
	if (toMove == 0 || (!move.bAllowPartial && toMove < move.count)) {
		LOG("Player %llu moved %u of %i between brick inventories but has %u", player.GetObjectID(), move.count, move.moveLOT, available);
		return;
	}

	EconomyLedger::ScopedItemTransfer moving;
	// The client already took them out of the source bag (0x00ce1310), so that side is silent
	Item* item = move.startObjectID != LWOOBJID_EMPTY ? inventory->FindItemById(move.startObjectID) : nullptr;
	uint32_t left = toMove;
	while (left > 0) {
		if (!item || item->GetInventory() != source || item->GetLot() != move.moveLOT) item = source->FindItemByLot(move.moveLOT);
		if (!item) break;
		const auto delta = std::min(item->GetCount(), left);
		left -= delta;
		item->SetCount(item->GetCount() - delta, true, false);
		item = nullptr;
	}

	const auto moved = toMove - left;
	if (moved == 0) return;
	inventory->ReceiveItem(LWOOBJID_EMPTY, move.moveLOT, moved, eLootSourceType::RELOCATE, {}, LWOOBJID_EMPTY, false,
		{ .inventory = move.dstBag, .showFlyingLoot = move.showFlyingLoot, .sourceInventory = move.srcBag });
}

void BrickByBrick::EndSession(Entity& player) {
	RecoverUnfinishedBuild(player);
}

void BrickByBrick::OnPlayerLoaded(Entity& player) {
	RecoverUnfinishedBuild(player);
}

uint32_t BrickByBrick::RecoverUnfinishedBuild(Entity& player) {
	auto* inventory = player.GetComponent<InventoryComponent>();
	if (!inventory) return 0;

	const auto autosave = Database::Get()->GetBbbAutosave(player.GetObjectID());
	auto recovery = PlanRecovery(IdsIn(*inventory, eInventoryType::MODELS_IN_BBB), autosave);
	const bool hadBricks = !ItemsIn(*inventory, eInventoryType::BRICKS_IN_BBB).empty();
	if (!autosave && recovery.giveBack.empty() && !hadBricks) return 0;

	uint32_t rebuilt = 0;
	if (recovery.rebuild) {
		for (const auto& model : StoreModels(player, autosave->lxfml, "autosave_" + std::to_string(player.GetObjectID()))) {
			inventory->ReceiveItem(LWOOBJID_EMPTY, MODEL_ITEM_LOT, 1, eLootSourceType::INVENTORY, ModelItemConfig(model.blueprintId, model.modelId), model.modelId, false,
				{ .inventory = eInventoryType::MODELS, .showFlyingLoot = false });
			rebuilt++;
		}
	}

	if (rebuilt > 0) {
		// The rebuilt models replace the ones that were open when the autosave was made (they may already be back in
		// MODELS after a reload)
		for (const auto id : recovery.consume) {
			auto* item = inventory->FindItemById(id);
			if (item) item->SetCount(0, false, false, false, eLootSourceType::INVENTORY);
		}
	} else {
		for (const auto id : recovery.consume) {
			if (std::ranges::find(recovery.giveBack, id) == recovery.giveBack.end()) recovery.giveBack.push_back(id);
		}
	}

	for (const auto id : recovery.giveBack) {
		auto* item = inventory->FindItemById(id);
		if (item && item->GetInventory()->GetType() == eInventoryType::MODELS_IN_BBB) MoveKeepingId(*inventory, item, eInventoryType::MODELS);
	}

	SettleBricks(*inventory, rebuilt > 0);
	if (autosave) Database::Get()->DeleteBbbAutosave(player.GetObjectID());

	LOG("Recovered the brick by brick build of %llu: %u models rebuilt, %zu models given back", player.GetObjectID(), rebuilt, recovery.giveBack.size());
	if (rebuilt > 0) {
		GameMessages::RebuildBBBAutosaveMsg message;
		message.target = player.GetObjectID();
		message.count = static_cast<int32_t>(rebuilt);
		message.SendToClient(player.GetSystemAddress());
	}
	return rebuilt;
}

std::vector<LWOOBJID> BrickByBrick::BehaviorIds(const std::string_view behaviors) {
	std::vector<LWOOBJID> ids(5, LWOOBJID_EMPTY);
	size_t index = 0;
	for (const auto& part : GeneralUtils::SplitString(std::string(behaviors), ',')) {
		if (index >= ids.size()) break;
		ids[index++] = GeneralUtils::TryParse<LWOOBJID>(part).value_or(LWOOBJID_EMPTY);
	}
	return ids;
}

std::u16string BrickByBrick::BrickList(const std::vector<LOT>& brickLots) {
	std::u16string list;
	for (const auto lot : brickLots) {
		list += GeneralUtils::to_u16string(lot);
		list += u':';
	}
	return list;
}

void BrickByBrick::FillModelMetadata(Entity& player, const LWOOBJID ugId, GameMessages::FetchModelMetadataResponse& response) {
	if (ugId == LWOOBJID_EMPTY) return;

	bool found = false;
	LWOOBJID blueprintId = LWOOBJID_EMPTY;
	std::string behaviors;
	std::u16string name;
	std::u16string description;

	// One of the player's model items: a brick built model keeps its UGID as the item's subkey
	auto* inventory = player.GetComponent<InventoryComponent>();
	if (inventory) {
		for (auto* bag : inventory->GetInventories() | std::views::values) {
			for (const auto* item : bag->GetItems() | std::views::values) {
				if (item->GetSubKey() != ugId) continue;
				const auto& config = item->GetConfig();
				blueprintId = ConfigObjectId(config, u"blueprintid");
				behaviors = ConfigText(config, u"userModelBehaviors");
				name = GeneralUtils::UTF8ToUTF16(ConfigText(config, u"userModelName"));
				description = GeneralUtils::UTF8ToUTF16(ConfigText(config, u"userModelDesc"));
				found = true;
				break;
			}
			if (found) break;
		}
	}

	// A model placed on a property (this one or one the player visits)
	if (!found) {
		const auto model = Database::Get()->GetModel(ugId);
		if (model) {
			found = true;
			blueprintId = model->ugcId;
			for (size_t i = 0; i < model->behaviors.size(); i++) {
				if (i != 0) behaviors += ',';
				behaviors += std::to_string(model->behaviors[i]);
			}
		}
	}
	if (!found) {
		LOG_DEBUG("Player %llu asked for the metadata of model %llu, which is neither theirs nor placed", player.GetObjectID(), ugId);
		return;
	}

	auto& ug = response.ugData;
	response.bHasUGData = true;
	ug.userModelID = ugId;
	ug.userModelName = name;
	ug.userModelDesc = description;
	ug.userModelBehaviors = BehaviorIds(behaviors);

	// A premade model (no blueprint) has no owner or blueprint data, as live sent it
	if (blueprintId == LWOOBJID_EMPTY) return;
	ug.blueprintID = blueprintId;
	const auto owner = Database::Get()->GetUgcProcessInfo(blueprintId);
	if (owner) {
		ug.owningPlayerID = owner->characterId;
		ug.owningPlayerName = GeneralUtils::UTF8ToUTF16(owner->characterName);
		const auto character = Database::Get()->GetCharacterInfo(owner->characterId);
		if (character) ug.accountID = character->accountId;
	}
	response.bHasBPData = FillBlueprintMetadata(blueprintId, response.bpData);
}
