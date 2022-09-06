#include "Item.h"

#include <sstream>

#include "../dWorldServer/ObjectIDManager.h"
#include "GeneralUtils.h"
#include "GameMessages.h"
#include "Entity.h"
#include "Game.h"
#include "dLogger.h"
#include "EntityManager.h"
#include "RenderComponent.h"
#include "PossessableComponent.h"
#include "CharacterComponent.h"
#include "eItemType.h"

class Inventory;


Item::Item(const LWOOBJID id, const LOT lot, Inventory* inventory, const uint32_t slot, const uint32_t count, const bool bound, const std::vector<LDFBaseData*>& config, const LWOOBJID parent, LWOOBJID subKey, eLootSourceType lootSourceType) {
	if (!Inventory::IsValidItem(lot)) {
		return;
	}

	this->id = id;
	this->lot = lot;
	this->inventory = inventory;
	this->slot = slot;
	this->count = count;
	this->bound = bound;
	this->config = config;
	this->parent = parent;
	this->info = &Inventory::FindItemComponent(lot);
	this->preconditions = new PreconditionExpression(this->info->reqPrecondition);
	this->subKey = subKey;

	inventory->AddManagedItem(this);
}

Item::Item(
	const LOT lot,
	Inventory* inventory,
	const uint32_t slot,
	const uint32_t count,
	const std::vector<LDFBaseData*>& config,
	const LWOOBJID parent,
	bool showFlyingLoot,
	bool isModMoveAndEquip,
	LWOOBJID subKey,
	bool bound,
	eLootSourceType lootSourceType) {
	if (!Inventory::IsValidItem(lot)) {
		return;
	}

	if (isModMoveAndEquip) {
		showFlyingLoot = false;
	}

	this->lot = lot;
	this->inventory = inventory;
	this->slot = slot;
	this->count = count;
	this->config = config;
	this->parent = parent;
	this->id = LWOOBJID_EMPTY;
	this->info = &Inventory::FindItemComponent(lot);
	this->bound = info->isBOP || bound;
	this->preconditions = new PreconditionExpression(this->info->reqPrecondition);
	this->subKey = subKey;

	LWOOBJID id = ObjectIDManager::GenerateRandomObjectID();

	id = GeneralUtils::SetBit(id, OBJECT_BIT_CHARACTER);
	id = GeneralUtils::SetBit(id, OBJECT_BIT_PERSISTENT);

	const auto type = static_cast<eItemType>(info->itemType);

	if (type == eItemType::ITEM_TYPE_MOUNT) {
		id = GeneralUtils::SetBit(id, OBJECT_BIT_CLIENT);
	}

	this->id = id;

	inventory->AddManagedItem(this);

	auto* entity = inventory->GetComponent()->GetParent();
	GameMessages::SendAddItemToInventoryClientSync(entity, entity->GetSystemAddress(), this, id, showFlyingLoot, static_cast<int>(this->count), subKey, lootSourceType);

	if (isModMoveAndEquip) {
		Equip();

		Game::logger->Log("Item", "Move and equipped (%i) from (%i)", this->lot, this->inventory->GetType());

		EntityManager::Instance()->SerializeEntity(inventory->GetComponent()->GetParent());
	}
}

LWOOBJID Item::GetId() const {
	return id;
}

LOT Item::GetLot() const {
	return lot;
}

uint32_t Item::GetCount() const {
	return count;
}

uint32_t Item::GetSlot() const {
	return slot;
}

std::vector<LDFBaseData*>& Item::GetConfig() {
	return config;
}

const CDItemComponent& Item::GetInfo() const {
	return *info;
}

bool Item::GetBound() const {
	return bound;
}

Inventory* Item::GetInventory() const {
	return inventory;
}

LWOOBJID Item::GetParent() const {
	return parent;
}

LWOOBJID Item::GetSubKey() const {
	return subKey;
}

PreconditionExpression* Item::GetPreconditionExpression() const {
	return preconditions;
}

void Item::SetCount(const uint32_t value, const bool silent, const bool disassemble, const bool showFlyingLoot, eLootSourceType lootSourceType) {
	if (value == count) {
		return;
	}

	const auto delta = std::abs(static_cast<int32_t>(value) - static_cast<int32_t>(count));

	const auto type = static_cast<eItemType>(info->itemType);

	if (disassemble) {
		if (value < count) {
			for (auto i = 0; i < delta; ++i) {
				Disassemble();
			}
		}
	}

	if (!silent) {
		auto* entity = inventory->GetComponent()->GetParent();

		if (value > count) {
			GameMessages::SendAddItemToInventoryClientSync(entity, entity->GetSystemAddress(), this, id, showFlyingLoot, delta, LWOOBJID_EMPTY, lootSourceType);
		} else {
			GameMessages::SendRemoveItemFromInventory(entity, entity->GetSystemAddress(), id, lot, inventory->GetType(), delta, value);
		}
	}

	count = value;

	if (count == 0) {
		RemoveFromInventory();
	}
}

void Item::SetSlot(const uint32_t value) {
	if (slot == value) {
		return;
	}

	for (const auto& pair : inventory->GetItems()) {
		auto* item = pair.second;

		if (item->slot == value) {
			item->slot = slot;
		}
	}

	slot = value;
}

void Item::SetBound(const bool value) {
	bound = value;
}

void Item::SetSubKey(LWOOBJID value) {
	subKey = value;
}

void Item::SetInventory(Inventory* value) {
	inventory->RemoveManagedItem(this);

	inventory = value;

	inventory->AddManagedItem(this);
}

void Item::Equip(const bool skipChecks) {
	if (IsEquipped()) {
		return;
	}

	inventory->GetComponent()->EquipItem(this, skipChecks);
}

void Item::UnEquip() {
	if (!IsEquipped()) {
		return;
	}

	inventory->GetComponent()->UnEquipItem(this);
}

bool Item::IsEquipped() const {
	auto* component = inventory->GetComponent();

	for (const auto& pair : component->GetEquippedItems()) {
		const auto item = pair.second;

		if (item.id == id) {
			return true;
		}
	}

	return false;
}

bool Item::Consume() {
	auto* skillsTable = CDClientManager::Instance()->GetTable<CDObjectSkillsTable>("ObjectSkills");

	auto skills = skillsTable->Query([=](const CDObjectSkills entry) {
		return entry.objectTemplate == static_cast<uint32_t>(lot);
		});

	auto success = false;

	for (auto& skill : skills) {
		if (skill.castOnType == 3) // Consumable type
		{
			success = true;
		}
	}

	Game::logger->Log("Item", "Consumed (%i) / (%llu) with (%d)", lot, id, success);

	GameMessages::SendUseItemResult(inventory->GetComponent()->GetParent(), lot, success);

	if (success) {
		inventory->GetComponent()->RemoveItem(lot, 1);
	}

	return success;
}

void Item::UseNonEquip() {
	const auto type = static_cast<eItemType>(info->itemType);
	if (type == eItemType::ITEM_TYPE_MOUNT) {
		GetInventory()->GetComponent()->HandlePossession(this);
	} else if (type == eItemType::ITEM_TYPE_PET_INVENTORY_ITEM && subKey != LWOOBJID_EMPTY) {
		const auto& databasePet = GetInventory()->GetComponent()->GetDatabasePet(subKey);
		if (databasePet.lot != LOT_NULL) {
			GetInventory()->GetComponent()->SpawnPet(this);
		}
	} else {
		auto* compRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
		const auto packageComponentId = compRegistryTable->GetByIDAndType(lot, COMPONENT_TYPE_PACKAGE);

		if (packageComponentId == 0) return;

		auto* packCompTable = CDClientManager::Instance()->GetTable<CDPackageComponentTable>("PackageComponent");
		auto packages = packCompTable->Query([=](const CDPackageComponent entry) {return entry.id == static_cast<uint32_t>(packageComponentId); });

		const auto success = !packages.empty();
		if (success) {
			auto* entityParent = inventory->GetComponent()->GetParent();
			for (auto& pack : packages) {
				std::unordered_map<LOT, int32_t> result{};
				result = LootGenerator::Instance().RollLootMatrix(entityParent, pack.LootMatrixIndex);
				if (!inventory->GetComponent()->HasSpaceForLoot(result)) {
				}
				LootGenerator::Instance().GiveLoot(inventory->GetComponent()->GetParent(), result, eLootSourceType::LOOT_SOURCE_CONSUMPTION);
			}
			inventory->GetComponent()->RemoveItem(lot, 1);
		}
	}
}

void Item::Disassemble(const eInventoryType inventoryType) {
	for (auto* data : config) {
		if (data->GetKey() == u"assemblyPartLOTs") {
			auto modStr = data->GetValueAsString();

			std::vector<LOT> modArray;

			std::stringstream ssData(modStr);

			std::string token;

			const auto deliminator = '+';

			while (std::getline(ssData, token, deliminator)) {
				const auto modLot = std::stoi(token.substr(2, token.size() - 1));

				modArray.push_back(modLot);
			}

			for (const auto mod : modArray) {
				inventory->GetComponent()->AddItem(mod, 1, eLootSourceType::LOOT_SOURCE_DELETION, inventoryType);
			}
		}
	}
}

void Item::DisassembleModel() {
	auto* table = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");

	const auto componentId = table->GetByIDAndType(GetLot(), COMPONENT_TYPE_RENDER);

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT render_asset FROM RenderComponent WHERE id = ?;");
	query.bind(1, (int)componentId);

	auto result = query.execQuery();

	if (result.eof()) {
		return;
	}

	std::string renderAsset = result.fieldIsNull(0) ? "" : std::string(result.getStringField(0));
	std::vector<std::string> renderAssetSplit = GeneralUtils::SplitString(renderAsset, '\\');

	std::string lxfmlPath = "res/BrickModels/" + GeneralUtils::SplitString(renderAssetSplit.back(), '.')[0] + ".lxfml";
	std::ifstream file(lxfmlPath);

	result.finalize();

	if (!file.good()) {
		return;
	}

	std::stringstream data;
	data << file.rdbuf();

	if (data.str().empty()) {
		return;
	}

	auto* doc = new tinyxml2::XMLDocument();

	if (!doc) {
		return;
	}

	if (doc->Parse(data.str().c_str(), data.str().size()) != 0) {
		return;
	}

	std::vector<int> parts;

	auto* lxfml = doc->FirstChildElement("LXFML");
	auto* bricks = lxfml->FirstChildElement("Bricks");
	std::string searchTerm = "Brick";

	if (!bricks) {
		searchTerm = "Part";
		bricks = lxfml->FirstChildElement("Scene")->FirstChildElement("Model")->FirstChildElement("Group");

		if (!bricks) {
			return;
		}
	}

	auto* currentBrick = bricks->FirstChildElement(searchTerm.c_str());
	while (currentBrick) {
		if (currentBrick->Attribute("designID") != nullptr) {
			parts.push_back(std::stoi(currentBrick->Attribute("designID")));
		}

		currentBrick = currentBrick->NextSiblingElement(searchTerm.c_str());
	}

	auto* brickIDTable = CDClientManager::Instance()->GetTable<CDBrickIDTableTable>("BrickIDTable");

	for (unsigned int part : parts) {
		const auto brickID = brickIDTable->Query([=](const CDBrickIDTable& entry) {
			return entry.LEGOBrickID == part;
			});

		if (brickID.empty()) {
			continue;
		}

		GetInventory()->GetComponent()->AddItem(brickID[0].NDObjectID, 1, eLootSourceType::LOOT_SOURCE_DELETION);
	}
}

void Item::RemoveFromInventory() {
	UnEquip();

	count = 0;

	inventory->RemoveManagedItem(this);

	delete this;
}

Item::~Item() {
	delete preconditions;

	for (auto* value : config) {
		delete value;
	}

	config.clear();
}
