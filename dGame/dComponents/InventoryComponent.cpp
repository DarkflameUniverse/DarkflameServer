#include "InventoryComponent.h"

#include <sstream>

#include "Entity.h"
#include "Item.h"
#include "Game.h"
#include "dLogger.h"
#include "CDClientManager.h"
#include "../dWorldServer/ObjectIDManager.h"
#include "MissionComponent.h"
#include "GameMessages.h"
#include "SkillComponent.h"
#include "Character.h"
#include "EntityManager.h"
#include "ItemSet.h"
#include "Player.h"
#include "PetComponent.h"
#include "PossessionComponent.h"
#include "PossessableComponent.h"
#include "ModuleAssemblyComponent.h"
#include "HavokVehiclePhysicsComponent.h"
#include "CharacterComponent.h"
#include "dZoneManager.h"
#include "PropertyManagementComponent.h"
#include "DestroyableComponent.h"
#include "dConfig.h"
#include "eItemType.h"
#include "eUnequippableActiveType.h"
#include "CppScripts.h"
#include "eMissionTaskType.h"
#include "eStateChangeType.h"
#include "eUseItemResponse.h"

#include "CDComponentsRegistryTable.h"
#include "CDInventoryComponentTable.h"
#include "CDScriptComponentTable.h"
#include "CDObjectSkillsTable.h"
#include "CDSkillBehaviorTable.h"

InventoryComponent::InventoryComponent(Entity* parent, tinyxml2::XMLDocument* document) : Component(parent) {
	this->m_Dirty = true;
	this->m_Equipped.clear();
	this->m_Pushed.clear();
	this->m_Consumable = LOT_NULL;
	this->m_Pets.clear();

	const auto lot = parent->GetLOT();

	if (m_ParentEntity->IsPlayer()) {
		LoadXml(document);

		CheckProxyIntegrity();

		return;
	}

	auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	const auto componentId = compRegistryTable->GetByIDAndType(lot, eReplicaComponentType::INVENTORY);

	auto* inventoryComponentTable = CDClientManager::Instance().GetTable<CDInventoryComponentTable>();
	auto items = inventoryComponentTable->Query([&componentId](const CDInventoryComponent entry) { return entry.id == componentId; });

	auto slot = 0u;

	for (const auto& item : items) {
		if (!item.equip || !Inventory::IsValidItem(item.itemid)) {
			continue;
		}

		const LWOOBJID id = ObjectIDManager::Instance()->GenerateObjectID();

		const auto& info = Inventory::FindItemComponent(item.itemid);

		UpdateSlot(info.equipLocation, EquippedItem(id, static_cast<LOT>(item.itemid), item.count, slot++));

		// Equip this items proxies.
		auto subItems = info.subItems;

		subItems.erase(std::remove_if(subItems.begin(), subItems.end(), ::isspace), subItems.end());

		if (subItems.empty()) return;
		const auto subItemsSplit = GeneralUtils::SplitString(subItems, ',');

		for (const auto& proxyLotAsString : subItemsSplit) {
			LOT proxyLot;
			if (GeneralUtils::TryParse(proxyLotAsString, proxyLot)) {
				Game::logger->Log("InventoryComponent", "Failed to parse %s to lot for entity %i:%llu", proxyLotAsString.c_str(), m_ParentEntity->GetLOT(), m_ParentEntity->GetObjectID());
				continue;
			}

			const auto& proxyInfo = Inventory::FindItemComponent(proxyLot);
			const LWOOBJID proxyId = ObjectIDManager::Instance()->GenerateObjectID();

			// Use item.count since we equip item.count number of the item this is a requested proxy of
			UpdateSlot(proxyInfo.equipLocation, EquippedItem(proxyId, proxyLot, item.count, slot++));
		}
	}
}

Inventory* InventoryComponent::GetInventory(const eInventoryType type) {
	const auto index = m_Inventories.find(type);

	if (index != m_Inventories.end()) return index->second;

	// Create new empty inventory
	uint32_t size = 240u;

	switch (type) {
	case eInventoryType::ITEMS:
		size = 20u;
		break;
	case eInventoryType::VAULT_MODELS:
	case eInventoryType::VAULT_ITEMS:
		size = 40u;
		break;
	case eInventoryType::VENDOR_BUYBACK:
		size = 27u;
		break;
	default:
		break;
	}

	auto* inventory = new Inventory(type, size, std::vector<Item*>(), this);

	m_Inventories.insert_or_assign(type, inventory);

	return inventory;
}

uint32_t InventoryComponent::GetLotCount(const LOT lot) const {
	uint32_t count = 0;

	for (const auto& [inventoryId, inventory] : m_Inventories) {
		count += inventory->GetLotCount(lot);
	}

	return count;
}

uint32_t InventoryComponent::GetLotCountNonTransfer(LOT lot) const {
	uint32_t count = 0;

	for (const auto [inventoryId, inventory] : m_Inventories) {
		if (IsTransferInventory(inventory->GetType())) continue;

		count += inventory->GetLotCount(lot);
	}

	return count;
}

void InventoryComponent::AddItem(
	const LOT lot,
	const uint32_t count,
	eLootSourceType lootSourceType,
	eInventoryType inventoryType,
	const std::vector<LDFBaseData*>& config,
	const LWOOBJID parent,
	const bool showFlyingLoot,
	bool isModMoveAndEquip,
	const LWOOBJID subKey,
	const eInventoryType inventorySourceType,
	const int32_t sourceType,
	const bool bound,
	int32_t preferredSlot) {
	if (count == 0) {
		Game::logger->Log("InventoryComponent", "Attempted to add 0 of item (%i) to the inventory of %i:%llu!", lot, m_ParentEntity->GetLOT(), m_ParentEntity->GetObjectID());
		return;
	}

	if (!Inventory::IsValidItem(lot)) {
		if (lot > 0) {
			Game::logger->Log("InventoryComponent", "Attempted to add invalid item (%i) to the inventory of %i:%llu!", lot, m_ParentEntity->GetLOT(), m_ParentEntity->GetObjectID());
		}
		return;
	}

	if (inventoryType == INVALID) inventoryType = Inventory::FindInventoryTypeForLot(lot);

	auto* missionComponent = m_ParentEntity->GetComponent<MissionComponent>();

	auto* inventory = GetInventory(inventoryType);

	// Config items cant stack as they are unique per item.
	if (!config.empty() || bound) {
		const auto slot = preferredSlot != -1 && inventory->IsSlotEmpty(preferredSlot) ? preferredSlot : inventory->FindEmptySlot();

		if (slot == -1) {
			Game::logger->Log("InventoryComponent", "Failed to find empty slot for inventory (%i) of %i:%llu!", inventoryType, m_ParentEntity->GetLOT(), m_ParentEntity->GetObjectID());

			return;
		}

		auto* item = new Item(lot, inventory, slot, count, config, parent, showFlyingLoot, isModMoveAndEquip, subKey, bound, lootSourceType);

		if (missionComponent && !IsTransferInventory(inventoryType)) {
			missionComponent->Progress(eMissionTaskType::GATHER, lot, LWOOBJID_EMPTY, "", count, IsTransferInventory(inventorySourceType));
		}

		return;
	}

	const auto& info = Inventory::FindItemComponent(lot);

	auto left = count;

	int32_t outOfSpace = 0;

	auto stack = static_cast<uint32_t>(info.stackSize);

	bool isBrick = inventoryType == eInventoryType::BRICKS || (stack == 0 && info.itemType == eItemType::BRICK);

	// info.itemType of 1 is item type brick
	if (isBrick) {
		stack = UINT32_MAX;
	} else if (stack == 0) {
		stack = 1;
	}

	auto* existing = FindItemByLot(lot, inventoryType);

	if (existing) {
		const auto delta = std::min<uint32_t>(left, stack - existing->GetCount());

		left -= delta;

		existing->SetCount(existing->GetCount() + delta, false, true, showFlyingLoot, lootSourceType);

		if (isModMoveAndEquip) {
			existing->Equip();

			isModMoveAndEquip = false;
		}
	}

	// If we have some leftover and we aren't bricks, make a new stack
	while (left > 0 && (!isBrick || (isBrick && !existing))) {
		const auto size = std::min(left, stack);

		left -= size;

		int32_t slot;

		if (preferredSlot != -1 && inventory->IsSlotEmpty(preferredSlot)) {
			slot = preferredSlot;

			preferredSlot = -1;
		} else {
			slot = inventory->FindEmptySlot();
		}

		if (slot == -1) {
			auto* player = dynamic_cast<Player*>(GetParentEntity());

			if (!player) return;

			outOfSpace += size;

			switch (sourceType) {
			case 0:
				player->SendMail(LWOOBJID_EMPTY, "Darkflame Universe", "Lost Reward", "You received an item and didn&apos;t have room for it.", lot, size);
				break;

			case 1:
				for (size_t i = 0; i < size; i++) {
					GameMessages::SendDropClientLoot(this->m_ParentEntity, this->m_ParentEntity->GetObjectID(), lot, 0, this->m_ParentEntity->GetPosition(), 1);
				}

				break;

			default:
				break;
			}

			continue;
		}
		auto* item = new Item(lot, inventory, slot, size, {}, parent, showFlyingLoot, isModMoveAndEquip, subKey, false, lootSourceType);

		isModMoveAndEquip = false;
	}

	if (!missionComponent || IsTransferInventory(inventoryType)) return;

	missionComponent->Progress(eMissionTaskType::GATHER, lot, LWOOBJID_EMPTY, "", count - outOfSpace, IsTransferInventory(inventorySourceType));
}

void InventoryComponent::RemoveItem(const LOT lot, const uint32_t count, eInventoryType inventoryType, const bool ignoreBound) {
	if (count == 0) {
		Game::logger->Log("InventoryComponent", "Attempted to remove 0 of item (%i) from the inventory of %i:%llu!", lot, m_ParentEntity->GetLOT(), m_ParentEntity->GetObjectID());

		return;
	}

	if (inventoryType == INVALID) {
		inventoryType = Inventory::FindInventoryTypeForLot(lot);
	}

	auto* inventory = GetInventory(inventoryType);

	if (!inventory) return;

	auto left = std::min<uint32_t>(count, inventory->GetLotCount(lot));

	while (left > 0) {
		auto* item = FindItemByLot(lot, inventoryType, false, ignoreBound);

		if (!item) break;

		const auto delta = std::min<uint32_t>(left, item->GetCount());

		item->SetCount(item->GetCount() - delta);

		left -= delta;
	}
}

void InventoryComponent::MoveItemToInventory(Item* item, const eInventoryType inventory, const uint32_t count, const bool showFlyingLot, bool isModMoveAndEquip, const bool ignoreEquipped, const int32_t preferredSlot) {
	if (!item) return;

	auto* origin = item->GetInventory();

	const auto lot = item->GetLot();

	const auto subkey = item->GetSubKey();

	if (subkey == LWOOBJID_EMPTY && item->GetConfig().empty() && (!item->GetBound() || (item->GetBound() && item->GetInfo().isBOP))) {
		auto left = std::min<uint32_t>(count, origin->GetLotCount(lot));

		while (left > 0) {
			if (!item) {
				item = origin->FindItemByLot(lot, false);

				if (!item) break;
			}

			const auto delta = std::min<uint32_t>(item->GetCount(), left);

			left -= delta;

			AddItem(lot, delta, eLootSourceType::NONE, inventory, {}, LWOOBJID_EMPTY, showFlyingLot, isModMoveAndEquip, LWOOBJID_EMPTY, origin->GetType(), 0, false, preferredSlot);

			item->SetCount(item->GetCount() - delta, false, false);

			isModMoveAndEquip = false;
		}
	} else {
		std::vector<LDFBaseData*> config;

		for (auto* const data : item->GetConfig()) {
			config.push_back(data->Copy());
		}

		const auto delta = std::min<uint32_t>(item->GetCount(), count);

		AddItem(lot, delta, eLootSourceType::NONE, inventory, config, LWOOBJID_EMPTY, showFlyingLot, isModMoveAndEquip, subkey, origin->GetType(), 0, item->GetBound(), preferredSlot);

		item->SetCount(item->GetCount() - delta, false, false);
	}

	auto* missionComponent = m_ParentEntity->GetComponent<MissionComponent>();

	if (missionComponent) {
		if (IsTransferInventory(inventory)) {
			missionComponent->Progress(eMissionTaskType::GATHER, lot, LWOOBJID_EMPTY, "", -static_cast<int32_t>(count));
		}
	}
}

void InventoryComponent::MoveStack(Item* item, const eInventoryType inventory, const uint32_t slot) {
	if (!item) return;

	if (inventory != INVALID && item->GetInventory()->GetType() != inventory) {
		auto* newInventory = GetInventory(inventory);

		item->SetInventory(newInventory);
	}

	item->SetSlot(slot);
}

Item* InventoryComponent::FindItemById(const LWOOBJID id) const {
	if (id == LWOOBJID_EMPTY) {
		Game::logger->Log("InventoryComponent", "Attempted to find item with empty id!");

		return nullptr;
	}
	for (const auto [inventoryType, inventory] : m_Inventories) {
		auto* item = inventory->FindItemById(id);

		if (item) return item;
	}

	return nullptr;
}

Item* InventoryComponent::FindItemByLot(const LOT lot, eInventoryType inventoryType, const bool ignoreEquipped, const bool ignoreBound) {
	if (inventoryType == INVALID) {
		inventoryType = Inventory::FindInventoryTypeForLot(lot);
	}

	auto* inventory = GetInventory(inventoryType);

	return inventory->FindItemByLot(lot, ignoreEquipped, ignoreBound);
}

Item* InventoryComponent::FindItemBySubKey(LWOOBJID id, eInventoryType inventoryType) {
	if (id == LWOOBJID_EMPTY) {
		Game::logger->Log("InventoryComponent", "Attempted to find item by SubKey with empty id!");

		return nullptr;
	}

	if (inventoryType == INVALID) {
		for (const auto& [inventoryId, inventory] : m_Inventories) {
			auto* item = inventory->FindItemBySubKey(id);

			if (item) return item;
		}

		return nullptr;
	} else {
		return GetInventory(inventoryType)->FindItemBySubKey(id);
	}
}

bool InventoryComponent::HasSpaceForLoot(const std::unordered_map<LOT, int32_t>& loot) {
	std::unordered_map<eInventoryType, int32_t> spaceOffset{};

	uint32_t slotsNeeded = 0;

	for (const auto& [item, count] : loot) {
		const auto inventoryType = Inventory::FindInventoryTypeForLot(item);

		if (inventoryType == BRICKS) continue;

		auto* inventory = GetInventory(inventoryType);

		if (!inventory) return false;

		const auto info = Inventory::FindItemComponent(item);

		auto stack = static_cast<uint32_t>(info.stackSize);

		auto left = count;

		auto* partial = inventory->FindItemByLot(item);

		if (partial && partial->GetCount() < stack) {
			left -= stack - partial->GetCount();
		}

		auto requiredSlots = std::ceil(static_cast<double>(left) / stack);

		const auto& offsetIter = spaceOffset.find(inventoryType);

		auto freeSpace = inventory->GetEmptySlots() - (offsetIter == spaceOffset.end() ? 0 : offsetIter->second);

		if (requiredSlots > freeSpace) {
			slotsNeeded += requiredSlots - freeSpace;
		}

		spaceOffset[inventoryType] = offsetIter == spaceOffset.end() ? requiredSlots : offsetIter->second + requiredSlots;
	}

	if (slotsNeeded > 0) {
		GameMessages::SendNotifyNotEnoughInvSpace(m_ParentEntity->GetObjectID(), slotsNeeded, ITEMS, m_ParentEntity->GetSystemAddress());

		return false;
	}

	return true;
}

void InventoryComponent::LoadXml(tinyxml2::XMLDocument* document) {
	LoadPetXml(document);

	auto* inventoryElement = document->FirstChildElement("obj")->FirstChildElement("inv");

	if (!inventoryElement) {
		Game::logger->Log("InventoryComponent", "Failed to find 'inv' xml element!");

		return;
	}

	auto* bags = inventoryElement->FirstChildElement("bag");

	if (!bags) {
		Game::logger->Log("InventoryComponent", "Failed to find 'bags' xml element!");

		return;
	}

	m_Consumable = inventoryElement->IntAttribute("csl", LOT_NULL);

	auto* bag = bags->FirstChildElement();

	while (bag) {
		unsigned int type;
		unsigned int size;

		bag->QueryAttribute("t", &type);
		bag->QueryAttribute("m", &size);

		auto* inventory = GetInventory(static_cast<eInventoryType>(type));

		inventory->SetSize(size);

		bag = bag->NextSiblingElement();
	}

	auto* items = inventoryElement->FirstChildElement("items");

	if (!items) {
		Game::logger->Log("InventoryComponent", "Failed to find 'items' xml element!");

		return;
	}

	bag = items->FirstChildElement();

	while (bag) {
		unsigned int type;

		bag->QueryAttribute("t", &type);

		auto* inventory = GetInventory(static_cast<eInventoryType>(type));

		if (inventory == nullptr) {
			Game::logger->Log("InventoryComponent", "Failed to find inventory (%i)!", type);

			return;
		}

		auto* itemElement = bag->FirstChildElement();

		while (itemElement) {
			LWOOBJID id;
			LOT lot;
			bool equipped;
			unsigned int slot;
			unsigned int count;
			bool bound;
			LWOOBJID subKey = LWOOBJID_EMPTY;

			itemElement->QueryAttribute("id", &id);
			itemElement->QueryAttribute("l", &lot);
			itemElement->QueryAttribute("eq", &equipped);
			itemElement->QueryAttribute("s", &slot);
			itemElement->QueryAttribute("c", &count);
			itemElement->QueryAttribute("b", &bound);
			itemElement->QueryAttribute("sk", &subKey);

			// Begin custom xml
			auto parent = LWOOBJID_EMPTY;

			itemElement->QueryAttribute("parent", &parent);
			// End custom xml

			std::vector<LDFBaseData*> config;

			auto* extraInfo = itemElement->FirstChildElement("x");

			if (extraInfo) {
				std::string modInfo = extraInfo->Attribute("ma");

				LDFBaseData* moduleAssembly = new LDFData<std::u16string>(u"assemblyPartLOTs", GeneralUtils::ASCIIToUTF16(modInfo.substr(2, modInfo.size() - 1)));

				config.push_back(moduleAssembly);
			}

			const auto* item = new Item(id, lot, inventory, slot, count, bound, config, parent, subKey);

			if (equipped) {
				const auto info = Inventory::FindItemComponent(lot);

				UpdateSlot(info.equipLocation, EquippedItem(item->GetId(), item->GetLot(), item->GetCount(), item->GetSlot()));

				AddItemSkills(item->GetLot());
			}

			itemElement = itemElement->NextSiblingElement();
		}

		bag = bag->NextSiblingElement();
	}

	for (const auto [inventoryId, inventory] : m_Inventories) {
		const auto itemCount = inventory->GetItems().size();

		if (inventory->GetSize() < itemCount) {
			inventory->SetSize(itemCount);
		}
	}
}

void InventoryComponent::UpdateXml(tinyxml2::XMLDocument* document) {
	UpdatePetXml(document);

	auto* inventoryElement = document->FirstChildElement("obj")->FirstChildElement("inv");

	if (inventoryElement == nullptr) {
		Game::logger->Log("InventoryComponent", "Failed to find 'inv' xml element!");

		return;
	}

	std::vector<Inventory*> inventoriesToSave;

	// Need to prevent some transfer inventories from being saved
	for (const auto [inventoryId, inventory] : this->m_Inventories) {
		if (inventory->GetType() == VENDOR_BUYBACK || inventory->GetType() == eInventoryType::MODELS_IN_BBB) {
			continue;
		}

		inventoriesToSave.push_back(inventory);
	}

	inventoryElement->SetAttribute("csl", m_Consumable);

	auto* bags = inventoryElement->FirstChildElement("bag");

	if (!bags) {
		Game::logger->Log("InventoryComponent", "Failed to find 'bags' xml element!");

		return;
	}

	bags->DeleteChildren();

	for (const auto* inventory : inventoriesToSave) {
		auto* bag = document->NewElement("b");

		bag->SetAttribute("t", inventory->GetType());
		bag->SetAttribute("m", static_cast<unsigned int>(inventory->GetSize()));

		bags->LinkEndChild(bag);
	}

	auto* items = inventoryElement->FirstChildElement("items");

	if (!items) {
		Game::logger->Log("InventoryComponent", "Failed to find 'items' xml element!");

		return;
	}

	items->DeleteChildren();

	for (auto* inventory : inventoriesToSave) {
		if (inventory->GetSize() == 0) continue;

		auto* bagElement = document->NewElement("in");

		bagElement->SetAttribute("t", inventory->GetType());

		for (const auto [itemObjId, item] : inventory->GetItems()) {
			auto* itemElement = document->NewElement("i");

			itemElement->SetAttribute("l", item->GetLot());
			itemElement->SetAttribute("id", item->GetId());
			itemElement->SetAttribute("s", static_cast<unsigned int>(item->GetSlot()));
			itemElement->SetAttribute("c", static_cast<unsigned int>(item->GetCount()));
			itemElement->SetAttribute("b", item->GetBound());
			itemElement->SetAttribute("eq", item->IsEquipped());
			itemElement->SetAttribute("sk", item->GetSubKey());

			// Begin custom xml
			itemElement->SetAttribute("parent", item->GetParentEntity());
			// End custom xml

			for (auto* data : item->GetConfig()) {
				if (data->GetKey() != u"assemblyPartLOTs") {
					continue;
				}

				auto* extraInfo = document->NewElement("x");

				extraInfo->SetAttribute("ma", data->GetString(false).c_str());

				itemElement->LinkEndChild(extraInfo);
			}

			bagElement->LinkEndChild(itemElement);
		}

		items->LinkEndChild(bagElement);
	}
}

void InventoryComponent::Serialize(RakNet::BitStream* outBitStream, const bool bIsInitialUpdate, unsigned& flags) {
	outBitStream->Write(bIsInitialUpdate || m_Dirty);
	if (bIsInitialUpdate || m_Dirty) {
		outBitStream->Write<uint32_t>(m_Equipped.size());

		for (const auto [itemObjId, item] : m_Equipped) {
			if (bIsInitialUpdate) AddItemSkills(item.lot);

			outBitStream->Write(item.id);
			outBitStream->Write(item.lot);

			outBitStream->Write0();

			outBitStream->Write(item.count > 0);
			if (item.count > 0) outBitStream->Write(item.count);

			outBitStream->Write(item.slot != 0);
			if (item.slot != 0) outBitStream->Write<uint16_t>(item.slot);

			outBitStream->Write0();

			bool flag = !item.config.empty();
			outBitStream->Write(flag);
			if (flag) {
				RakNet::BitStream ldfStream;
				ldfStream.Write<int32_t>(item.config.size()); // Key count
				for (LDFBaseData* data : item.config) {
					if (data->GetKey() == u"assemblyPartLOTs") {
						std::string newRocketStr = data->GetValueAsString() + ";";
						GeneralUtils::ReplaceInString(newRocketStr, "+", ";");
						LDFData<std::u16string>* ldf_data = new LDFData<std::u16string>(u"assemblyPartLOTs", GeneralUtils::ASCIIToUTF16(newRocketStr));
						ldf_data->WriteToPacket(&ldfStream);
						delete ldf_data;
					} else {
						data->WriteToPacket(&ldfStream);
					}
				}
				outBitStream->Write(ldfStream.GetNumberOfBytesUsed() + 1);
				outBitStream->Write<uint8_t>(0); // Don't compress
				outBitStream->Write(ldfStream);
			}

			outBitStream->Write1();
		}

		if (!bIsInitialUpdate) m_Dirty = false;
	}

	outBitStream->Write0(); // Equipped model transforms
}

void InventoryComponent::Update(float deltaTime) {
	for (auto* set : m_Itemsets) {
		set->Update(deltaTime);
	}
}

void InventoryComponent::UpdateSlot(const std::string& location, EquippedItem item, bool keepCurrent) {
	const auto index = m_Equipped.find(location);

	if (index != m_Equipped.end()) {
		if (keepCurrent) {
			m_Equipped.insert_or_assign(location + std::to_string(m_Equipped.size()), item);

			m_Dirty = true;

			return;
		}

		auto* old = FindItemById(index->second.id);

		if (old) {
			UnEquipItem(old);
		}
	}

	m_Equipped.insert_or_assign(location, item);

	m_Dirty = true;
}

void InventoryComponent::RemoveSlot(const std::string& location) {
	if (m_Equipped.find(location) == m_Equipped.end()) return;

	m_Equipped.erase(location);

	m_Dirty = true;
}

void InventoryComponent::EquipItem(Item* item, const bool skipChecks) {
	if (!Inventory::IsValidItem(item->GetLot())) return;

	// Temp items should be equippable but other transfer items shouldn't be (for example the instruments in RB)
	if (item->IsEquipped()
		|| (item->GetInventory()->GetType() != TEMP_ITEMS && IsTransferInventory(item->GetInventory()->GetType()))
		|| IsPet(item->GetSubKey())) {
		return;
	}

	auto* character = m_ParentEntity->GetCharacter();

	if (character && !skipChecks) {
		// Hacky proximity rocket
		if (item->GetLot() == LOT_ROCKET) {
			const auto rocketLauchPads = EntityManager::Instance()->GetEntitiesByComponent(eReplicaComponentType::ROCKET_LAUNCHPAD_CONTROL);

			const auto position = m_ParentEntity->GetPosition();

			for (auto* launchPad : rocketLauchPads) {
				if (!launchPad) continue;

				auto prereq = launchPad->GetVarAsString(u"rocketLaunchPreCondition");
				if (!prereq.empty()) {
					PreconditionExpression expression(prereq);
					if (!expression.Check(m_ParentEntity)) continue;
				}

				if (Vector3::DistanceSquared(launchPad->GetPosition(), position) > 13 * 13) continue;

				auto* characterComponent = m_ParentEntity->GetComponent<CharacterComponent>();

				if (characterComponent) characterComponent->SetLastRocketItemID(item->GetId());

				launchPad->OnUse(m_ParentEntity);

				break;
			}

			return;
		}

		const auto building = character->GetBuildMode();

		const auto type = item->GetInfo().itemType;


		if (!building && (item->GetLot() == LOT_THINKING_CAP || type == eItemType::LOOT_MODEL || type == eItemType::VEHICLE)) return;

		if (type != eItemType::LOOT_MODEL && type != eItemType::MODEL) {
			if (!item->GetBound() && !item->GetPreconditionExpression()->Check(m_ParentEntity)) {
				return;
			}
		}
	}

	const auto lot = item->GetLot();

	CheckItemSet(lot);

	for (auto* set : m_Itemsets) {
		set->OnEquip(lot);
	}

	if (item->GetInfo().isBOE) item->SetBound(true);

	GenerateProxies(item);

	UpdateSlot(item->GetInfo().equipLocation, EquippedItem(item->GetId(), item->GetLot(), item->GetCount(), item->GetSlot(), item->GetConfig()));

	ApplyBuff(item);

	AddItemSkills(item->GetLot());

	EquipScripts(item);

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);
}

void InventoryComponent::UnEquipItem(Item* item) {
	if (!item->IsEquipped()) return;

	const auto lot = item->GetLot();

	if (!Inventory::IsValidItem(lot)) return;

	CheckItemSet(lot);

	for (auto* set : m_Itemsets) {
		set->OnUnEquip(lot);
	}

	RemoveBuff(item);

	RemoveItemSkills(item->GetLot());

	RemoveSlot(item->GetInfo().equipLocation);

	PurgeProxies(item);

	UnequipScripts(item);

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	// Trigger property event
	if (PropertyManagementComponent::Instance() != nullptr && item->GetCount() > 0 && Inventory::FindInventoryTypeForLot(item->GetLot()) == MODELS) {
		PropertyManagementComponent::Instance()->GetParentEntity()->OnZonePropertyModelRemovedWhileEquipped(m_ParentEntity);
		dZoneManager::Instance()->GetZoneControlObject()->OnZonePropertyModelRemovedWhileEquipped(m_ParentEntity);
	}
}


void InventoryComponent::EquipScripts(Item* equippedItem) const {
	auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();

	int32_t scriptComponentID = compRegistryTable->GetByIDAndType(equippedItem->GetLot(), eReplicaComponentType::SCRIPT, -1);
	if (scriptComponentID > -1) {
		auto* scriptCompTable = CDClientManager::Instance().GetTable<CDScriptComponentTable>();
		auto scriptCompData = scriptCompTable->GetByID(scriptComponentID);
		auto* itemScript = CppScripts::GetScript(m_ParentEntity, scriptCompData.script_name);
		DluAssert(itemScript != nullptr);
		itemScript->OnFactionTriggerItemEquipped(m_ParentEntity, equippedItem->GetId());
	}
}

void InventoryComponent::UnequipScripts(Item* unequippedItem) const {
	auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();

	int32_t scriptComponentID = compRegistryTable->GetByIDAndType(unequippedItem->GetLot(), eReplicaComponentType::SCRIPT, -1);
	if (scriptComponentID > -1) {
		auto* scriptCompTable = CDClientManager::Instance().GetTable<CDScriptComponentTable>();
		auto scriptCompData = scriptCompTable->GetByID(scriptComponentID);
		auto* itemScript = CppScripts::GetScript(m_ParentEntity, scriptCompData.script_name);
		DluAssert(itemScript != nullptr);
		itemScript->OnFactionTriggerItemUnequipped(m_ParentEntity, unequippedItem->GetId());
	}
}

void InventoryComponent::HandlePossession(Item* item) const {
	auto* characterComponent = m_ParentEntity->GetComponent<CharacterComponent>();
	if (!characterComponent) return;

	auto* possessionComponent = m_ParentEntity->GetComponent<PossessionComponent>();
	if (!possessionComponent) return;

	// Don't do anything if we are busy dismounting
	if (possessionComponent->GetIsDismounting()) return;

	// Check to see if we are already mounting something
	auto* currentlyPossessedEntity = EntityManager::Instance()->GetEntity(possessionComponent->GetPossessable());
	auto currentlyPossessedItem = possessionComponent->GetMountItemID();

	if (currentlyPossessedItem) {
		if (currentlyPossessedEntity) possessionComponent->Dismount(currentlyPossessedEntity);
		return;
	}

	GameMessages::SendSetStunned(m_ParentEntity->GetObjectID(), eStateChangeType::PUSH, m_ParentEntity->GetSystemAddress(), LWOOBJID_EMPTY, true, false, true, false, false, false, false, true, true, true, true, true, true, true, true, true);

	// Set the mount Item ID so that we know what were handling
	possessionComponent->SetMountItemID(item->GetId());
	GameMessages::SendSetMountInventoryID(m_ParentEntity, item->GetId(), UNASSIGNED_SYSTEM_ADDRESS);

	// Create entity to mount
	auto startRotation = m_ParentEntity->GetRotation();

	EntityInfo info{};
	info.lot = item->GetLot();
	info.pos = m_ParentEntity->GetPosition();
	info.rot = startRotation;
	info.spawnerID = m_ParentEntity->GetObjectID();

	auto* mount = EntityManager::Instance()->CreateEntity(info, nullptr, m_ParentEntity);

	// Check to see if the mount is a vehicle, if so, flip it
	auto* havokVehiclePhysicsComponent = mount->GetComponent<HavokVehiclePhysicsComponent>();
	if (havokVehiclePhysicsComponent) characterComponent->SetIsRacing(true);

	// Setup the destroyable stats
	auto* destroyableComponent = mount->GetComponent<DestroyableComponent>();
	if (destroyableComponent) destroyableComponent->SetIsImmune(true);

	// Mount it
	auto* possessableComponent = mount->GetComponent<PossessableComponent>();
	if (possessableComponent) {
		possessableComponent->SetIsItemSpawned(true);
		possessableComponent->SetPossessor(m_ParentEntity->GetObjectID());
		// Possess it
		possessionComponent->SetPossessable(mount->GetObjectID());
		possessionComponent->SetPossessableType(possessableComponent->GetPossessionType());
	}

	GameMessages::SendSetJetPackMode(m_ParentEntity, false);

	// Make it go to the client
	EntityManager::Instance()->ConstructEntity(mount);
	// Update the possessor
	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	// have to unlock the input so it vehicle can be driven
	if (havokVehiclePhysicsComponent) GameMessages::SendVehicleUnlockInput(mount->GetObjectID(), false, m_ParentEntity->GetSystemAddress());
	GameMessages::SendMarkInventoryItemAsActive(m_ParentEntity->GetObjectID(), true, eUnequippableActiveType::MOUNT, item->GetId(), m_ParentEntity->GetSystemAddress());
}

void InventoryComponent::ApplyBuff(Item* item) const {
	const auto buffs = FindBuffs(item, true);

	for (const auto buff : buffs) {
		SkillComponent::HandleUnmanaged(buff, m_ParentEntity->GetObjectID());
	}
}

// TODO Something needs to send the remove buff GameMessage as well when it is unequipping items that would remove buffs.
void InventoryComponent::RemoveBuff(Item* item) const {
	const auto buffs = FindBuffs(item, false);

	for (const auto buff : buffs) {
		SkillComponent::HandleUnCast(buff, m_ParentEntity->GetObjectID());
	}
}

void InventoryComponent::PushEquippedItems() {
	m_Pushed = m_Equipped;

	m_Dirty = true;
}

void InventoryComponent::PopEquippedItems() {
	auto current = m_Equipped;

	for (const auto& pair : current) {
		auto* const item = FindItemById(pair.second.id);

		if (item) item->UnEquip();
	}

	for (const auto& pair : m_Pushed) {
		auto* const item = FindItemById(pair.second.id);

		if (item) item->Equip();
	}

	m_Pushed.clear();

	auto* destroyableComponent = m_ParentEntity->GetComponent<DestroyableComponent>();

	// Reset stats to full
	if (destroyableComponent) {
		destroyableComponent->SetHealth(static_cast<int32_t>(destroyableComponent->GetMaxHealth()));
		destroyableComponent->SetArmor(static_cast<int32_t>(destroyableComponent->GetMaxArmor()));
		destroyableComponent->SetImagination(static_cast<int32_t>(destroyableComponent->GetMaxImagination()));
		EntityManager::Instance()->SerializeEntity(m_ParentEntity);
	}

	m_Dirty = true;
}


bool InventoryComponent::IsEquipped(const LOT lot) const {
	for (const auto& [equipLocation, equippedItem] : m_Equipped) {
		if (equippedItem.lot == lot) {
			return true;
		}
	}

	return false;
}

void InventoryComponent::CheckItemSet(const LOT lot) {
	// Check if the lot is in the item set cache
	if (std::find(m_ItemSetsChecked.begin(), m_ItemSetsChecked.end(), lot) != m_ItemSetsChecked.end()) return;

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT setID FROM ItemSets WHERE itemIDs LIKE %?%;");
	query.bind(1, lot);

	auto result = query.execQuery();

	while (!result.eof()) {
		const auto id = result.getIntField(0);

		bool found = false;

		// Check if we have the set already
		for (auto* itemset : m_Itemsets) {
			if (itemset->GetID() == id) {
				found = true;
				break;
			}
		}

		if (!found) {
			auto* set = new ItemSet(id, this);

			m_Itemsets.push_back(set);
		}

		result.nextRow();
	}

	m_ItemSetsChecked.push_back(lot);
}

void InventoryComponent::AddItemSkills(const LOT lot) {
	const auto info = Inventory::FindItemComponent(lot);

	const auto slot = FindBehaviorSlot(info.itemType);

	if (slot == BehaviorSlot::Invalid) return;

	const auto index = m_Skills.find(slot);

	const auto skill = FindSkill(lot);

	if (skill == 0) return;

	if (index != m_Skills.end()) {
		const auto old = index->second;

		GameMessages::SendRemoveSkill(m_ParentEntity, old);
	}

	GameMessages::SendAddSkill(m_ParentEntity, skill, static_cast<int>(slot));

	m_Skills.insert_or_assign(slot, skill);
}

void InventoryComponent::RemoveItemSkills(const LOT lot) {
	const auto info = Inventory::FindItemComponent(lot);

	const auto slot = FindBehaviorSlot(info.itemType);

	if (slot == BehaviorSlot::Invalid) return;

	const auto index = m_Skills.find(slot);

	if (index == m_Skills.end()) return;

	const auto skillId = index->second;

	GameMessages::SendRemoveSkill(m_ParentEntity, skillId);

	m_Skills.erase(slot);

	if (slot != BehaviorSlot::Primary) return;

	m_Skills.insert_or_assign(BehaviorSlot::Primary, 1);
	GameMessages::SendAddSkill(m_ParentEntity, 1, static_cast<int>(BehaviorSlot::Primary));
}

void InventoryComponent::TriggerPassiveAbility(PassiveAbilityTrigger trigger, Entity* target) const {
	for (auto* set : m_Itemsets) {
		set->TriggerPassiveAbility(trigger, target);
	}
}

bool InventoryComponent::HasAnyPassive(const std::vector<eItemSetPassiveAbilityID>& passiveIDs, int32_t equipmentRequirement) const {
	for (auto* set : m_Itemsets) {
		if (set->GetEquippedCount() < equipmentRequirement) {
			continue;
		}

		// Check if the set has any of the passive abilities
		if (std::find(passiveIDs.begin(), passiveIDs.end(), static_cast<eItemSetPassiveAbilityID>(set->GetID())) != passiveIDs.end()) {
			return true;
		}
	}

	return false;
}

void InventoryComponent::DespawnPet() {
	auto current = PetComponent::GetActivePet(m_ParentEntity->GetObjectID());
	if (!current) return;
	current->Deactivate();
}

void InventoryComponent::SpawnPet(Item* item) {
	auto current = PetComponent::GetActivePet(m_ParentEntity->GetObjectID());

	if (current) {
		current->Deactivate();

		if (current->GetDatabaseId() == item->GetSubKey()) {
			return;
		}
	}

	// First check if we can summon the pet.  You need 1 imagination to do so.
	auto* destroyableComponent = m_ParentEntity->GetComponent<DestroyableComponent>();

	if (Game::config->GetValue("pets_take_imagination") == "1" && destroyableComponent && destroyableComponent->GetImagination() <= 0) {
		GameMessages::SendUseItemRequirementsResponse(m_ParentEntity->GetObjectID(), m_ParentEntity->GetSystemAddress(), eUseItemResponse::NoImaginationForPet);
		return;
	}

	EntityInfo info{};
	info.lot = item->GetLot();
	info.pos = m_ParentEntity->GetPosition();
	info.rot = NiQuaternion::IDENTITY;
	info.spawnerID = m_ParentEntity->GetObjectID();

	auto* pet = EntityManager::Instance()->CreateEntity(info);

	auto* petComponent = pet->GetComponent<PetComponent>();

	if (petComponent) petComponent->Activate(item);

	EntityManager::Instance()->ConstructEntity(pet);
}

void InventoryComponent::SetDatabasePet(LWOOBJID id, const DatabasePet& data) {
	m_Pets.insert_or_assign(id, data);
}

const DatabasePet& InventoryComponent::GetDatabasePet(LWOOBJID id) const {
	const auto& pair = m_Pets.find(id);

	return pair == m_Pets.end() ? DATABASE_PET_INVALID : pair->second;
}

BehaviorSlot InventoryComponent::FindBehaviorSlot(const eItemType type) {
	switch (type) {
	case eItemType::HAT:
		return BehaviorSlot::Head;
	case eItemType::NECK:
		return BehaviorSlot::Neck;
	case eItemType::LEFT_HAND:
		return BehaviorSlot::Offhand;
	case eItemType::RIGHT_HAND:
		return BehaviorSlot::Primary;
	case eItemType::CONSUMABLE:
		return BehaviorSlot::Consumable;
	default:
		return BehaviorSlot::Invalid;
	}
}

bool InventoryComponent::IsTransferInventory(eInventoryType type) {
	return type == VENDOR_BUYBACK || type == VAULT_ITEMS || type == VAULT_MODELS || type == TEMP_ITEMS || type == TEMP_MODELS || type == MODELS_IN_BBB;
}

uint32_t InventoryComponent::FindSkill(const LOT lot) {
	auto* table = CDClientManager::Instance().GetTable<CDObjectSkillsTable>();

	const auto results = table->Query([&lot](const CDObjectSkills& entry) {
		return entry.objectTemplate == static_cast<uint32_t>(lot);
		});

	for (const auto& result : results) {
		if (result.castOnType == 0) {
			return result.skillID;
		}
	}

	return 0;
}

std::vector<uint32_t> InventoryComponent::FindBuffs(Item* item, bool castOnEquip) const {
	std::vector<uint32_t> buffs;
	if (!item) return buffs;
	auto* table = CDClientManager::Instance().GetTable<CDObjectSkillsTable>();
	auto* behaviors = CDClientManager::Instance().GetTable<CDSkillBehaviorTable>();

	const auto results = table->Query([item](const CDObjectSkills& entry) {
		return entry.objectTemplate == static_cast<unsigned int>(item->GetLot());
		});

	auto* missions = m_ParentEntity->GetComponent<MissionComponent>();

	for (const auto& result : results) {
		if (result.castOnType != 1) continue;

		const auto entry = behaviors->GetSkillByID(result.skillID);

		if (entry.skillID == 0) {
			Game::logger->Log("InventoryComponent", "Failed to find buff behavior for skill (%i)!", result.skillID);

			continue;
		}

		if (missions && castOnEquip) {
			missions->Progress(eMissionTaskType::USE_SKILL, result.skillID);
		}

		// If item is not a proxy, add its buff to the added buffs.
		if (item->GetParentEntity() == LWOOBJID_EMPTY) buffs.push_back(static_cast<uint32_t>(entry.behaviorID));
	}

	return buffs;
}

void InventoryComponent::SetNPCItems(const std::vector<LOT>& items) {
	m_Equipped.clear();

	auto slot = 0u;

	for (const auto& item : items) {
		const LWOOBJID id = ObjectIDManager::Instance()->GenerateObjectID();

		const auto& info = Inventory::FindItemComponent(item);

		UpdateSlot(info.equipLocation, EquippedItem(id, static_cast<LOT>(item), 1, slot++), true);
	}

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);
}

InventoryComponent::~InventoryComponent() {
	for (const auto& [inventoryId, inventory] : m_Inventories) {
		delete inventory;
	}

	m_Inventories.clear();

	std::for_each(m_Itemsets.begin(), m_Itemsets.end(), [](ItemSet* set) { delete set; });

	m_Itemsets.clear();
	m_Pets.clear();
}

std::vector<Item*> InventoryComponent::GenerateProxies(Item* parent) {
	std::vector<Item*> proxies;

	auto subItems = parent->GetInfo().subItems;

	if (subItems.empty()) return proxies;

	subItems.erase(std::remove_if(subItems.begin(), subItems.end(), ::isspace), subItems.end());

	auto itemsAsStr = GeneralUtils::SplitString(subItems, ',');

	std::vector<int> lots;
	std::for_each(itemsAsStr.begin(), itemsAsStr.end(), [&lots](const std::string& str) {
		int32_t lot;
		if (GeneralUtils::TryParse(str, lot)) lots.push_back(lot);
		else Game::logger->Log("InventoryComponent", "failed to parse %s to a lot.", str.c_str());
		});

	std::for_each(lots.begin(), lots.end(), [&proxies, parent, this](const int lot) {
		if (!Inventory::IsValidItem(lot)) return;

		auto* inventory = GetInventory(ITEM_SETS);

		auto* proxy = new Item(lot, inventory, inventory->FindEmptySlot(), 1, {}, parent->GetId(), false);

		EquipItem(proxy);

		proxies.push_back(proxy);
		});

	return proxies;
}

std::vector<Item*> InventoryComponent::FindProxies(const LWOOBJID parent) {
	auto* inventory = GetInventory(ITEM_SETS);

	std::vector<Item*> proxies;
	for (const auto& [itemObjId, item] : inventory->GetItems()) {
		if (item && item->GetParentEntity() != parent) proxies.push_back(item);
	}

	return proxies;
}

bool InventoryComponent::IsValidProxy(const LWOOBJID parent) {
	for (const auto& [inventoryType, inventory] : m_Inventories) {
		if (!inventory)	continue;
		for (const auto& [itemObjId, item] : inventory->GetItems()) {
			if (item->GetId() == parent) return true;
		}
	}

	return false;
}

bool InventoryComponent::IsParentValid(Item* root) {
	if (root->GetInfo().subItems.empty()) return true;

	const auto id = root->GetId();

	for (const auto& [inventoryType, inventory] : m_Inventories) {
		if (!inventory)	continue;
		for (const auto& [itemObjId, item] : inventory->GetItems()) {
			if (item->GetParentEntity() == id) return true;
		}
	}

	return false;
}

void InventoryComponent::CheckProxyIntegrity() {
	std::vector<Item*> dead;

	for (const auto& [inventoryType, inventory] : m_Inventories) {
		if (!inventory)	continue;
		for (const auto& [itemObjId, item] : inventory->GetItems()) {
			const auto parent = item->GetParentEntity();

			if (parent != LWOOBJID_EMPTY && !IsValidProxy(parent)) dead.push_back(item);
		}
	}

	std::for_each(dead.begin(), dead.end(), [](Item* item) { if (item) item->RemoveFromInventory(); });
}

void InventoryComponent::PurgeProxies(Item* item) {
	const auto root = item->GetParentEntity();

	if (root != LWOOBJID_EMPTY) {
		item = FindItemById(root);

		if (item) {
			UnEquipItem(item);
		}

		return;
	}

	auto proxies = FindProxies(item->GetId());

	std::for_each(proxies.begin(), proxies.end(), [](Item* proxy) {
		if (!proxy) return;
		proxy->UnEquip();
		proxy->RemoveFromInventory();
		});
}

void InventoryComponent::LoadPetXml(tinyxml2::XMLDocument* document) {
	auto* petInventoryElement = document->FirstChildElement("obj")->FirstChildElement("pet");

	if (!petInventoryElement) {
		m_Pets.clear();

		return;
	}

	auto* petElement = petInventoryElement->FirstChildElement();

	while (petElement) {
		LWOOBJID id;
		LOT lot;
		int32_t moderationStatus;

		petElement->QueryAttribute("id", &id);
		petElement->QueryAttribute("l", &lot);
		petElement->QueryAttribute("m", &moderationStatus);
		const char* name = petElement->Attribute("n");

		DatabasePet databasePet;
		databasePet.lot = lot;
		databasePet.moderationState = moderationStatus;
		databasePet.name = std::string(name);

		SetDatabasePet(id, databasePet);

		petElement = petElement->NextSiblingElement();
	}
}

void InventoryComponent::UpdatePetXml(tinyxml2::XMLDocument* document) {
	auto* petInventoryElement = document->FirstChildElement("obj")->FirstChildElement("pet");

	if (petInventoryElement == nullptr) {
		petInventoryElement = document->NewElement("pet");

		document->FirstChildElement("obj")->LinkEndChild(petInventoryElement);
	}

	petInventoryElement->DeleteChildren();

	for (const auto& [petObjId, pet] : m_Pets) {
		auto* petElement = document->NewElement("p");

		petElement->SetAttribute("id", petObjId);
		petElement->SetAttribute("l", pet.lot);
		petElement->SetAttribute("m", pet.moderationState);
		petElement->SetAttribute("n", pet.name.c_str());
		petElement->SetAttribute("t", 0);

		petInventoryElement->LinkEndChild(petElement);
	}
}
