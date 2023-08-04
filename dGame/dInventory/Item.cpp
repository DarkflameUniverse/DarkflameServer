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
#include "AssetManager.h"
#include "InventoryComponent.h"
#include "Loot.h"
#include "eObjectBits.h"
#include "eReplicaComponentType.h"
#include "eUseItemResponse.h"

#include "CDBrickIDTableTable.h"
#include "CDObjectSkillsTable.h"
#include "CDComponentsRegistryTable.h"
#include "CDPackageComponentTable.h"

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

	GeneralUtils::SetBit(id, eObjectBits::CHARACTER);
	GeneralUtils::SetBit(id, eObjectBits::PERSISTENT);

	const auto type = static_cast<eItemType>(info->itemType);

	if (type == eItemType::MOUNT) {
		GeneralUtils::SetBit(id, eObjectBits::CLIENT);
	}

	this->id = id;

	inventory->AddManagedItem(this);

	auto* entity = inventory->GetComponent()->GetParent();
	GameMessages::SendAddItemToInventoryClientSync(entity, entity->GetSystemAddress(), this, id, showFlyingLoot, static_cast<int>(this->count), subKey, lootSourceType);

	if (isModMoveAndEquip) {
		Equip();

		Game::logger->Log("Item", "Move and equipped (%i) from (%i)", this->lot, this->inventory->GetType());

		Game::entityManager->SerializeEntity(inventory->GetComponent()->GetParent());
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
	auto* skillsTable = CDClientManager::Instance().GetTable<CDObjectSkillsTable>();

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

	Game::logger->LogDebug("Item", "Consumed LOT (%i) itemID (%llu).  Success=(%d)", lot, id, success);

	GameMessages::SendUseItemResult(inventory->GetComponent()->GetParent(), lot, success);

	if (success) {
		inventory->GetComponent()->RemoveItem(lot, 1);
	}

	return success;
}

void Item::UseNonEquip(Item* item) {
	LOT thisLot = this->GetLot();
	if (!GetInventory()) {
		Game::logger->LogDebug("Item", "item %i has no inventory??", this->GetLot());
		return;
	}

	auto* playerInventoryComponent = GetInventory()->GetComponent();
	if (!playerInventoryComponent) {
		Game::logger->LogDebug("Item", "no inventory component attached to item id %llu lot %i", this->GetId(), this->GetLot());
		return;
	}

	auto* playerEntity = playerInventoryComponent->GetParent();
	if (!playerEntity) {
		Game::logger->LogDebug("Item", "no player entity attached to inventory? item id is %llu", this->GetId());
		return;
	}

	const auto type = static_cast<eItemType>(info->itemType);
	if (type == eItemType::MOUNT) {
		playerInventoryComponent->HandlePossession(this);
		// TODO Check if mounts are allowed to be spawned
	} else if (type == eItemType::PET_INVENTORY_ITEM && subKey != LWOOBJID_EMPTY) {
		const auto& databasePet = playerInventoryComponent->GetDatabasePet(subKey);
		if (databasePet.lot != LOT_NULL) {
			playerInventoryComponent->SpawnPet(this);
		}
		// This precondition response is taken care of in SpawnPet().
	} else {
		bool success = false;
		auto inventory = item->GetInventory();
		if (inventory && inventory->GetType() == eInventoryType::ITEMS) {
			auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
			const auto packageComponentId = compRegistryTable->GetByIDAndType(lot, eReplicaComponentType::PACKAGE);

			if (packageComponentId == 0) return;

			auto* packCompTable = CDClientManager::Instance().GetTable<CDPackageComponentTable>();
			auto packages = packCompTable->Query([=](const CDPackageComponent entry) {return entry.id == static_cast<uint32_t>(packageComponentId); });

			auto success = !packages.empty();
			if (success) {
				if (this->GetPreconditionExpression()->Check(playerInventoryComponent->GetParent())) {
					auto* entityParent = playerInventoryComponent->GetParent();
					// Roll the loot for all the packages then see if it all fits.  If it fits, give it to the player, otherwise don't.
					std::unordered_map<LOT, int32_t> rolledLoot{};
					for (auto& pack : packages) {
						auto thisPackage = LootGenerator::Instance().RollLootMatrix(entityParent, pack.LootMatrixIndex);
						for (auto& loot : thisPackage) {
							// If we already rolled this lot, add it to the existing one, otherwise create a new entry.
							auto existingLoot = rolledLoot.find(loot.first);
							if (existingLoot == rolledLoot.end()) {
								rolledLoot.insert(loot);
							} else {
								existingLoot->second += loot.second;
							}
						}
					}
					if (playerInventoryComponent->HasSpaceForLoot(rolledLoot)) {
						LootGenerator::Instance().GiveLoot(playerInventoryComponent->GetParent(), rolledLoot, eLootSourceType::CONSUMPTION);
						item->SetCount(item->GetCount() - 1);
					} else {
						success = false;
					}
				} else {
					GameMessages::SendUseItemRequirementsResponse(
						playerInventoryComponent->GetParent()->GetObjectID(),
						playerInventoryComponent->GetParent()->GetSystemAddress(),
						eUseItemResponse::FailedPrecondition
					);
					success = false;
				}
			}
		}
		Game::logger->LogDebug("Item", "Player %llu %s used item %i", playerEntity->GetObjectID(), success ? "successfully" : "unsuccessfully", thisLot);
		GameMessages::SendUseItemResult(playerInventoryComponent->GetParent(), thisLot, success);
	}
}

void Item::Disassemble(const eInventoryType inventoryType) {
	for (auto* data : config) {
		if (data->GetKey() == u"assemblyPartLOTs") {
			auto modStr = data->GetValueAsString();

			// This shouldn't be null but always check your pointers.
			if (GetInventory()) {
				auto inventoryComponent = GetInventory()->GetComponent();
				if (inventoryComponent) {
					auto entity = inventoryComponent->GetParent();
					if (entity) entity->SetVar<std::string>(u"currentModifiedBuild", modStr);
				}
			}

			std::vector<LOT> modArray;

			std::stringstream ssData(modStr);

			std::string token;

			const auto deliminator = '+';

			while (std::getline(ssData, token, deliminator)) {
				const auto modLot = std::stoi(token.substr(2, token.size() - 1));

				modArray.push_back(modLot);
			}

			for (const auto mod : modArray) {
				inventory->GetComponent()->AddItem(mod, 1, eLootSourceType::DELETION, inventoryType);
			}
		}
	}
}

void Item::DisassembleModel() {
	auto* table = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();

	const auto componentId = table->GetByIDAndType(GetLot(), eReplicaComponentType::RENDER);

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT render_asset, LXFMLFolder FROM RenderComponent WHERE id = ?;");
	query.bind(1, (int)componentId);

	auto result = query.execQuery();

	if (result.eof() || result.fieldIsNull(0)) {
		return;
	}

	std::string renderAsset = std::string(result.getStringField(0));
	std::string lxfmlFolderName = std::string(result.getStringField(1));

	std::vector<std::string> renderAssetSplit = GeneralUtils::SplitString(renderAsset, '\\');
	if (renderAssetSplit.size() == 0) return;

	std::string lxfmlPath = "BrickModels/" + lxfmlFolderName + "/" + GeneralUtils::SplitString(renderAssetSplit.back(), '.').at(0) + ".lxfml";
	auto buffer = Game::assetManager->GetFileAsBuffer(lxfmlPath.c_str());

	if (!buffer.m_Success) {
		Game::logger->Log("Item", "Failed to load %s to disassemble model into bricks, check that this file exists", lxfmlPath.c_str());
		return;
	}

	std::istream file(&buffer);

	result.finalize();

	if (!file.good()) {
		buffer.close();
		return;
	}

	std::stringstream data;
	data << file.rdbuf();

	buffer.close();

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

	auto* brickIDTable = CDClientManager::Instance().GetTable<CDBrickIDTableTable>();

	for (unsigned int part : parts) {
		const auto brickID = brickIDTable->Query([=](const CDBrickIDTable& entry) {
			return entry.LEGOBrickID == part;
			});

		if (brickID.empty()) {
			continue;
		}

		GetInventory()->GetComponent()->AddItem(brickID[0].NDObjectID, 1, eLootSourceType::DELETION);
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
