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
#include "PossessorComponent.h"
#include "PossessableComponent.h"
#include "ModuleAssemblyComponent.h"
#include "VehiclePhysicsComponent.h"
#include "CharacterComponent.h"
#include "dZoneManager.h"
#include "PropertyManagementComponent.h"
#include "DestroyableComponent.h"
#include "dConfig.h"
#include "eItemType.h"
#include "eUnequippableActiveType.h"

InventoryComponent::InventoryComponent(Entity* parent, tinyxml2::XMLDocument* document) : Component(parent) {
	this->m_Dirty = true;
	this->m_Equipped = {};
	this->m_Pushed = {};
	this->m_Consumable = LOT_NULL;
	this->m_Pets = {};

	const auto lot = parent->GetLOT();

	if (lot == 1) {
		LoadXml(document);

		CheckProxyIntegrity();

		return;
	}

	auto* compRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
	const auto componentId = compRegistryTable->GetByIDAndType(lot, COMPONENT_TYPE_INVENTORY);

	auto* inventoryComponentTable = CDClientManager::Instance()->GetTable<CDInventoryComponentTable>("InventoryComponent");
	auto items = inventoryComponentTable->Query([=](const CDInventoryComponent entry) { return entry.id == componentId; });

	auto slot = 0u;

	for (const auto& item : items) {
		if (!item.equip || !Inventory::IsValidItem(item.itemid)) {
			continue;
		}

		const LWOOBJID id = ObjectIDManager::Instance()->GenerateObjectID();

		const auto& info = Inventory::FindItemComponent(item.itemid);

		UpdateSlot(info.equipLocation, { id, static_cast<LOT>(item.itemid), item.count, slot++ });

		// Equip this items proxies.
		auto subItems = info.subItems;

		subItems.erase(std::remove_if(subItems.begin(), subItems.end(), ::isspace), subItems.end());

		if (!subItems.empty()) {
			const auto subItemsSplit = GeneralUtils::SplitString(subItems, ',');

			for (auto proxyLotAsString : subItemsSplit) {
				const auto proxyLOT = static_cast<LOT>(std::stoi(proxyLotAsString));

				const auto& proxyInfo = Inventory::FindItemComponent(proxyLOT);
				const LWOOBJID proxyId = ObjectIDManager::Instance()->GenerateObjectID();

				// Use item.count since we equip item.count number of the item this is a requested proxy of
				UpdateSlot(proxyInfo.equipLocation, { proxyId, proxyLOT, item.count, slot++ });
			}
		}
	}
}

Inventory* InventoryComponent::GetInventory(const eInventoryType type) {
	const auto index = m_Inventories.find(type);

	if (index != m_Inventories.end()) {
		return index->second;
	}

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

	auto* inventory = new Inventory(type, size, {}, this);

	m_Inventories.insert_or_assign(type, inventory);

	return inventory;
}

const std::map<eInventoryType, Inventory*>& InventoryComponent::GetInventories() const {
	return m_Inventories;
}

uint32_t InventoryComponent::GetLotCount(const LOT lot) const {
	uint32_t count = 0;

	for (const auto& inventory : m_Inventories) {
		count += inventory.second->GetLotCount(lot);
	}

	return count;
}

uint32_t InventoryComponent::GetLotCountNonTransfer(LOT lot) const {
	uint32_t count = 0;

	for (const auto& inventory : m_Inventories) {
		if (IsTransferInventory(inventory.second->GetType())) continue;

		count += inventory.second->GetLotCount(lot);
	}

	return count;
}

const EquipmentMap& InventoryComponent::GetEquippedItems() const {
	return m_Equipped;
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
		Game::logger->Log("InventoryComponent", "Attempted to add 0 of item (%i) to the inventory!", lot);

		return;
	}

	if (!Inventory::IsValidItem(lot)) {
		if (lot > 0) {
			Game::logger->Log("InventoryComponent", "Attempted to add invalid item (%i) to the inventory!", lot);
		}

		return;
	}

	if (inventoryType == INVALID) {
		inventoryType = Inventory::FindInventoryTypeForLot(lot);
	}

	auto* missions = static_cast<MissionComponent*>(this->m_Parent->GetComponent(COMPONENT_TYPE_MISSION));

	auto* inventory = GetInventory(inventoryType);

	if (!config.empty() || bound) {
		const auto slot = preferredSlot != -1 && inventory->IsSlotEmpty(preferredSlot) ? preferredSlot : inventory->FindEmptySlot();

		if (slot == -1) {
			Game::logger->Log("InventoryComponent", "Failed to find empty slot for inventory (%i)!", inventoryType);

			return;
		}

		auto* item = new Item(lot, inventory, slot, count, config, parent, showFlyingLoot, isModMoveAndEquip, subKey, bound, lootSourceType);

		if (missions != nullptr && !IsTransferInventory(inventoryType)) {
			missions->Progress(MissionTaskType::MISSION_TASK_TYPE_ITEM_COLLECTION, lot, LWOOBJID_EMPTY, "", count, IsTransferInventory(inventorySourceType));
		}

		return;
	}

	const auto info = Inventory::FindItemComponent(lot);

	auto left = count;

	int32_t outOfSpace = 0;

	auto stack = static_cast<uint32_t>(info.stackSize);

	// info.itemType of 1 is item type brick
	if (inventoryType == eInventoryType::BRICKS || (stack == 0 && info.itemType == 1)) {
		stack = 999;
	} else if (stack == 0) {
		stack = 1;
	}

	auto* existing = FindItemByLot(lot, inventoryType);

	if (existing != nullptr) {
		const auto delta = std::min<uint32_t>(left, stack - existing->GetCount());

		left -= delta;

		existing->SetCount(existing->GetCount() + delta, false, true, showFlyingLoot, lootSourceType);

		if (isModMoveAndEquip) {
			existing->Equip();

			isModMoveAndEquip = false;
		}
	}

	while (left > 0) {
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
			auto* player = dynamic_cast<Player*>(GetParent());

			if (player == nullptr) {
				return;
			}

			outOfSpace += size;

			switch (sourceType) {
			case 0:
				player->SendMail(LWOOBJID_EMPTY, "Darkflame Universe", "Lost Reward", "You received an item and didn&apos;t have room for it.", lot, size);
				break;

			case 1:
				for (size_t i = 0; i < size; i++) {
					GameMessages::SendDropClientLoot(this->m_Parent, this->m_Parent->GetObjectID(), lot, 0, this->m_Parent->GetPosition(), 1);
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

	if (missions != nullptr && !IsTransferInventory(inventoryType)) {
		missions->Progress(MissionTaskType::MISSION_TASK_TYPE_ITEM_COLLECTION, lot, LWOOBJID_EMPTY, "", count - outOfSpace, IsTransferInventory(inventorySourceType));
	}
}

void InventoryComponent::RemoveItem(const LOT lot, const uint32_t count, eInventoryType inventoryType, const bool ignoreBound) {
	if (count == 0) {
		Game::logger->Log("InventoryComponent", "Attempted to remove 0 of item (%i) from the inventory!", lot);

		return;
	}

	if (inventoryType == INVALID) {
		inventoryType = Inventory::FindInventoryTypeForLot(lot);
	}

	auto* inventory = GetInventory(inventoryType);

	if (inventory == nullptr) {
		return;
	}

	auto left = std::min<uint32_t>(count, inventory->GetLotCount(lot));

	while (left > 0) {
		auto* item = FindItemByLot(lot, inventoryType, false, ignoreBound);

		if (item == nullptr) {
			break;
		}

		const auto delta = std::min<uint32_t>(left, item->GetCount());

		item->SetCount(item->GetCount() - delta);

		left -= delta;
	}
}

void InventoryComponent::MoveItemToInventory(Item* item, const eInventoryType inventory, const uint32_t count, const bool showFlyingLot, bool isModMoveAndEquip, const bool ignoreEquipped, const int32_t preferredSlot) {
	if (item == nullptr) {
		return;
	}

	auto* origin = item->GetInventory();

	const auto lot = item->GetLot();

	if (item->GetConfig().empty() && !item->GetBound() || (item->GetBound() && item->GetInfo().isBOP)) {
		auto left = std::min<uint32_t>(count, origin->GetLotCount(lot));

		while (left > 0) {
			if (item == nullptr) {
				item = origin->FindItemByLot(lot, false);

				if (item == nullptr) {
					break;
				}
			}

			const auto delta = std::min<uint32_t>(item->GetCount(), left);

			left -= delta;

			AddItem(lot, delta, eLootSourceType::LOOT_SOURCE_NONE, inventory, {}, LWOOBJID_EMPTY, showFlyingLot, isModMoveAndEquip, LWOOBJID_EMPTY, origin->GetType(), 0, false, preferredSlot);

			item->SetCount(item->GetCount() - delta, false, false);

			isModMoveAndEquip = false;
		}
	} else {
		std::vector<LDFBaseData*> config;

		for (auto* const data : item->GetConfig()) {
			config.push_back(data->Copy());
		}

		const auto delta = std::min<uint32_t>(item->GetCount(), count);

		AddItem(lot, delta, eLootSourceType::LOOT_SOURCE_NONE, inventory, config, LWOOBJID_EMPTY, showFlyingLot, isModMoveAndEquip, LWOOBJID_EMPTY, origin->GetType(), 0, item->GetBound(), preferredSlot);

		item->SetCount(item->GetCount() - delta, false, false);
	}

	auto* missionComponent = m_Parent->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		if (IsTransferInventory(inventory)) {
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_ITEM_COLLECTION, lot, LWOOBJID_EMPTY, "", -static_cast<int32_t>(count));
		}
	}
}

void InventoryComponent::MoveStack(Item* item, const eInventoryType inventory, const uint32_t slot) {
	if (inventory != INVALID && item->GetInventory()->GetType() != inventory) {
		auto* newInventory = GetInventory(inventory);

		item->SetInventory(newInventory);
	}

	item->SetSlot(slot);
}

Item* InventoryComponent::FindItemById(const LWOOBJID id) const {
	for (const auto& inventory : m_Inventories) {
		auto* item = inventory.second->FindItemById(id);

		if (item != nullptr) {
			return item;
		}
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
	if (inventoryType == INVALID) {
		for (const auto& inventory : m_Inventories) {
			auto* item = inventory.second->FindItemBySubKey(id);

			if (item != nullptr) {
				return item;
			}
		}

		return nullptr;
	} else {
		return GetInventory(inventoryType)->FindItemBySubKey(id);
	}
}

bool InventoryComponent::HasSpaceForLoot(const std::unordered_map<LOT, int32_t>& loot) {
	std::unordered_map<eInventoryType, int32_t> spaceOffset{};

	uint32_t slotsNeeded = 0;

	for (const auto& pair : loot) {
		const auto inventoryType = Inventory::FindInventoryTypeForLot(pair.first);

		if (inventoryType == BRICKS) {
			continue;
		}

		auto* inventory = GetInventory(inventoryType);

		if (inventory == nullptr) {
			return false;
		}

		const auto info = Inventory::FindItemComponent(pair.first);

		auto stack = static_cast<uint32_t>(info.stackSize);

		auto left = pair.second;

		auto* partial = inventory->FindItemByLot(pair.first);

		if (partial != nullptr && partial->GetCount() < stack) {
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
		GameMessages::SendNotifyNotEnoughInvSpace(m_Parent->GetObjectID(), slotsNeeded, ITEMS, m_Parent->GetSystemAddress());

		return false;
	}

	return true;
}

void InventoryComponent::LoadXml(tinyxml2::XMLDocument* document) {
	LoadPetXml(document);

	auto* inventoryElement = document->FirstChildElement("obj")->FirstChildElement("inv");

	if (inventoryElement == nullptr) {
		Game::logger->Log("InventoryComponent", "Failed to find 'inv' xml element!");

		return;
	}

	auto* bags = inventoryElement->FirstChildElement("bag");

	if (bags == nullptr) {
		Game::logger->Log("InventoryComponent", "Failed to find 'bags' xml element!");

		return;
	}

	m_Consumable = inventoryElement->IntAttribute("csl", LOT_NULL);

	auto* bag = bags->FirstChildElement();

	while (bag != nullptr) {
		unsigned int type;
		unsigned int size;

		bag->QueryAttribute("t", &type);
		bag->QueryAttribute("m", &size);

		auto* inventory = GetInventory(static_cast<eInventoryType>(type));

		inventory->SetSize(size);

		bag = bag->NextSiblingElement();
	}

	auto* items = inventoryElement->FirstChildElement("items");

	if (items == nullptr) {
		Game::logger->Log("InventoryComponent", "Failed to find 'items' xml element!");

		return;
	}

	bag = items->FirstChildElement();

	while (bag != nullptr) {
		unsigned int type;

		bag->QueryAttribute("t", &type);

		auto* inventory = GetInventory(static_cast<eInventoryType>(type));

		if (inventory == nullptr) {
			Game::logger->Log("InventoryComponent", "Failed to find inventory (%i)!", type);

			return;
		}

		auto* itemElement = bag->FirstChildElement();

		while (itemElement != nullptr) {
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

				UpdateSlot(info.equipLocation, { item->GetId(), item->GetLot(), item->GetCount(), item->GetSlot() });

				AddItemSkills(item->GetLot());
			}

			itemElement = itemElement->NextSiblingElement();
		}

		bag = bag->NextSiblingElement();
	}

	for (const auto inventory : m_Inventories) {
		const auto itemCount = inventory.second->GetItems().size();

		if (inventory.second->GetSize() < itemCount) {
			inventory.second->SetSize(itemCount);
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

	std::vector<Inventory*> inventories;

	for (const auto& pair : this->m_Inventories) {
		auto* inventory = pair.second;

		if (inventory->GetType() == VENDOR_BUYBACK) {
			continue;
		}

		inventories.push_back(inventory);
	}

	inventoryElement->SetAttribute("csl", m_Consumable);

	auto* bags = inventoryElement->FirstChildElement("bag");

	if (bags == nullptr) {
		Game::logger->Log("InventoryComponent", "Failed to find 'bags' xml element!");

		return;
	}

	bags->DeleteChildren();

	for (const auto* inventory : inventories) {
		auto* bag = document->NewElement("b");

		bag->SetAttribute("t", inventory->GetType());
		bag->SetAttribute("m", static_cast<unsigned int>(inventory->GetSize()));

		bags->LinkEndChild(bag);
	}

	auto* items = inventoryElement->FirstChildElement("items");

	if (items == nullptr) {
		Game::logger->Log("InventoryComponent", "Failed to find 'items' xml element!");

		return;
	}

	items->DeleteChildren();

	for (auto* inventory : inventories) {
		if (inventory->GetSize() == 0) {
			continue;
		}

		auto* bagElement = document->NewElement("in");

		bagElement->SetAttribute("t", inventory->GetType());

		for (const auto& pair : inventory->GetItems()) {
			auto* item = pair.second;

			auto* itemElement = document->NewElement("i");

			itemElement->SetAttribute("l", item->GetLot());
			itemElement->SetAttribute("id", item->GetId());
			itemElement->SetAttribute("s", static_cast<unsigned int>(item->GetSlot()));
			itemElement->SetAttribute("c", static_cast<unsigned int>(item->GetCount()));
			itemElement->SetAttribute("b", item->GetBound());
			itemElement->SetAttribute("eq", item->IsEquipped());
			itemElement->SetAttribute("sk", item->GetSubKey());

			// Begin custom xml
			itemElement->SetAttribute("parent", item->GetParent());
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
	if (bIsInitialUpdate || m_Dirty) {
		outBitStream->Write(true);

		outBitStream->Write<uint32_t>(m_Equipped.size());

		for (const auto& pair : m_Equipped) {
			const auto item = pair.second;

			if (bIsInitialUpdate) {
				AddItemSkills(item.lot);
			}

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

		m_Dirty = false;
	} else {
		outBitStream->Write(false);
	}

	outBitStream->Write(false);
}

void InventoryComponent::ResetFlags() {
	m_Dirty = false;
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

		if (old != nullptr) {
			UnEquipItem(old);
		}
	}

	m_Equipped.insert_or_assign(location, item);

	m_Dirty = true;
}

void InventoryComponent::RemoveSlot(const std::string& location) {
	if (m_Equipped.find(location) == m_Equipped.end()) {
		return;
	}

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

	auto* character = m_Parent->GetCharacter();

	if (character != nullptr && !skipChecks) {
		// Hacky proximity rocket
		if (item->GetLot() == 6416) {
			const auto rocketLauchPads = EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_ROCKET_LAUNCH);

			const auto position = m_Parent->GetPosition();

			for (auto* lauchPad : rocketLauchPads) {
				if (Vector3::DistanceSquared(lauchPad->GetPosition(), position) > 13 * 13) continue;

				auto* characterComponent = m_Parent->GetComponent<CharacterComponent>();

				if (characterComponent != nullptr) characterComponent->SetLastRocketItemID(item->GetId());

				lauchPad->OnUse(m_Parent);

				break;
			}

			return;
		}

		const auto building = character->GetBuildMode();

		const auto type = static_cast<eItemType>(item->GetInfo().itemType);


		if (!building && (item->GetLot() == 6086 || type == eItemType::ITEM_TYPE_LOOT_MODEL || type == eItemType::ITEM_TYPE_VEHICLE)) return;

		if (type != eItemType::ITEM_TYPE_LOOT_MODEL && type != eItemType::ITEM_TYPE_MODEL) {
			if (!item->GetBound() && !item->GetPreconditionExpression()->Check(m_Parent)) {
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

	UpdateSlot(item->GetInfo().equipLocation, { item->GetId(), item->GetLot(), item->GetCount(), item->GetSlot(), item->GetConfig() });

	ApplyBuff(item);

	AddItemSkills(item->GetLot());

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void InventoryComponent::UnEquipItem(Item* item) {
	if (!item->IsEquipped()) {
		return;
	}

	const auto lot = item->GetLot();

	if (!Inventory::IsValidItem(lot)) {
		return;
	}

	CheckItemSet(lot);

	for (auto* set : m_Itemsets) {
		set->OnUnEquip(lot);
	}

	RemoveBuff(item);

	RemoveItemSkills(item->GetLot());

	RemoveSlot(item->GetInfo().equipLocation);

	PurgeProxies(item);

	EntityManager::Instance()->SerializeEntity(m_Parent);

	// Trigger property event
	if (PropertyManagementComponent::Instance() != nullptr && item->GetCount() > 0 && Inventory::FindInventoryTypeForLot(item->GetLot()) == MODELS) {
		PropertyManagementComponent::Instance()->GetParent()->OnZonePropertyModelRemovedWhileEquipped(m_Parent);
		dZoneManager::Instance()->GetZoneControlObject()->OnZonePropertyModelRemovedWhileEquipped(m_Parent);
	}
}

void InventoryComponent::HandlePossession(Item* item) {
	auto* characterComponent = m_Parent->GetComponent<CharacterComponent>();
	if (!characterComponent) return;

	auto* possessorComponent = m_Parent->GetComponent<PossessorComponent>();
	if (!possessorComponent) return;

	// Don't do anything if we are busy dismounting
	if (possessorComponent->GetIsDismounting()) return;

	// Check to see if we are already mounting something
	auto* currentlyPossessedEntity = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());
	auto currentlyPossessedItem = possessorComponent->GetMountItemID();

	if (currentlyPossessedItem) {
		if (currentlyPossessedEntity) possessorComponent->Dismount(currentlyPossessedEntity);
		return;
	}

	GameMessages::SendSetStunned(m_Parent->GetObjectID(), eStunState::PUSH, m_Parent->GetSystemAddress(), LWOOBJID_EMPTY, true, false, true, false, false, false, false, true, true, true, true, true, true, true, true, true);

	// Set the mount Item ID so that we know what were handling
	possessorComponent->SetMountItemID(item->GetId());
	GameMessages::SendSetMountInventoryID(m_Parent, item->GetId(), UNASSIGNED_SYSTEM_ADDRESS);

	// Create entity to mount
	auto startRotation = m_Parent->GetRotation();

	EntityInfo info{};
	info.lot = item->GetLot();
	info.pos = m_Parent->GetPosition();
	info.rot = startRotation;
	info.spawnerID = m_Parent->GetObjectID();

	auto* mount = EntityManager::Instance()->CreateEntity(info, nullptr, m_Parent);

	// Check to see if the mount is a vehicle, if so, flip it
	auto* vehicleComponent = mount->GetComponent<VehiclePhysicsComponent>();
	if (vehicleComponent) {
		auto angles = startRotation.GetEulerAngles();
		// Make it right side up
		angles.x -= PI;
		// Make it going in the direction of the player
		angles.y -= PI;
		startRotation = NiQuaternion::FromEulerAngles(angles);
		mount->SetRotation(startRotation);
		// We're pod racing now
		characterComponent->SetIsRacing(true);
	}

	// Setup the destroyable stats
	auto* destroyableComponent = mount->GetComponent<DestroyableComponent>();
	if (destroyableComponent) {
		destroyableComponent->SetIsSmashable(false);
		destroyableComponent->SetIsImmune(true);
	}

	// Mount it
	auto* possessableComponent = mount->GetComponent<PossessableComponent>();
	if (possessableComponent) {
		possessableComponent->SetIsItemSpawned(true);
		possessableComponent->SetPossessor(m_Parent->GetObjectID());
		// Possess it
		possessorComponent->SetPossessable(mount->GetObjectID());
		possessorComponent->SetPossessableType(possessableComponent->GetPossessionType());
	}

	GameMessages::SendSetJetPackMode(m_Parent, false);

	// Make it go to the client
	EntityManager::Instance()->ConstructEntity(mount);
	// Update the possessor
	EntityManager::Instance()->SerializeEntity(m_Parent);

	// have to unlock the input so it vehicle can be driven
	if (vehicleComponent) GameMessages::SendVehicleUnlockInput(mount->GetObjectID(), false, m_Parent->GetSystemAddress());
	GameMessages::SendMarkInventoryItemAsActive(m_Parent->GetObjectID(), true, eUnequippableActiveType::MOUNT, item->GetId(), m_Parent->GetSystemAddress());
}

void InventoryComponent::ApplyBuff(Item* item) const {
	const auto buffs = FindBuffs(item, true);

	for (const auto buff : buffs) {
		SkillComponent::HandleUnmanaged(buff, m_Parent->GetObjectID());
	}
}

void InventoryComponent::RemoveBuff(Item* item) const {
	const auto buffs = FindBuffs(item, false);

	for (const auto buff : buffs) {
		SkillComponent::HandleUnCast(buff, m_Parent->GetObjectID());
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

		if (item == nullptr) {
			continue;
		}

		item->UnEquip();
	}

	for (const auto& pair : m_Pushed) {
		auto* const item = FindItemById(pair.second.id);

		if (item == nullptr) {
			continue;
		}

		item->Equip();
	}

	m_Pushed.clear();

	auto destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();

	// Reset stats to full
	if (destroyableComponent) {
		destroyableComponent->SetHealth(static_cast<int32_t>(destroyableComponent->GetMaxHealth()));
		destroyableComponent->SetArmor(static_cast<int32_t>(destroyableComponent->GetMaxArmor()));
		destroyableComponent->SetImagination(static_cast<int32_t>(destroyableComponent->GetMaxImagination()));
		EntityManager::Instance()->SerializeEntity(m_Parent);
	}

	m_Dirty = true;
}


bool InventoryComponent::IsEquipped(const LOT lot) const {
	for (const auto& pair : m_Equipped) {
		if (pair.second.lot == lot) {
			return true;
		}
	}

	return false;
}

void InventoryComponent::CheckItemSet(const LOT lot) {
	// Check if the lot is in the item set cache
	if (std::find(m_ItemSetsChecked.begin(), m_ItemSetsChecked.end(), lot) != m_ItemSetsChecked.end()) {
		return;
	}

	const std::string lot_query = "%" + std::to_string(lot) + "%";

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT setID FROM ItemSets WHERE itemIDs LIKE ?;");
	query.bind(1, lot_query.c_str());

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

	result.finalize();
}

void InventoryComponent::SetConsumable(LOT lot) {
	m_Consumable = lot;
}

LOT InventoryComponent::GetConsumable() const {
	return m_Consumable;
}

void InventoryComponent::AddItemSkills(const LOT lot) {
	const auto info = Inventory::FindItemComponent(lot);

	const auto slot = FindBehaviorSlot(static_cast<eItemType>(info.itemType));

	if (slot == BehaviorSlot::Invalid) {
		return;
	}

	const auto index = m_Skills.find(slot);

	const auto skill = FindSkill(lot);

	if (skill == 0) {
		return;
	}

	if (index != m_Skills.end()) {
		const auto old = index->second;

		GameMessages::SendRemoveSkill(m_Parent, old);
	}

	GameMessages::SendAddSkill(m_Parent, skill, static_cast<int>(slot));

	m_Skills.insert_or_assign(slot, skill);
}

void InventoryComponent::RemoveItemSkills(const LOT lot) {
	const auto info = Inventory::FindItemComponent(lot);

	const auto slot = FindBehaviorSlot(static_cast<eItemType>(info.itemType));

	if (slot == BehaviorSlot::Invalid) {
		return;
	}

	const auto index = m_Skills.find(slot);

	if (index == m_Skills.end()) {
		return;
	}

	const auto old = index->second;

	GameMessages::SendRemoveSkill(m_Parent, old);

	m_Skills.erase(slot);

	if (slot == BehaviorSlot::Primary) {
		m_Skills.insert_or_assign(BehaviorSlot::Primary, 1);

		GameMessages::SendAddSkill(m_Parent, 1, static_cast<int>(BehaviorSlot::Primary));
	}
}

void InventoryComponent::TriggerPassiveAbility(PassiveAbilityTrigger trigger) {
	for (auto* set : m_Itemsets) {
		set->TriggerPassiveAbility(trigger);
	}
}

bool InventoryComponent::HasAnyPassive(const std::vector<ItemSetPassiveAbilityID>& passiveIDs, int32_t equipmentRequirement) const {
	for (auto* set : m_Itemsets) {
		if (set->GetEquippedCount() < equipmentRequirement) {
			continue;
		}

		// Check if the set has any of the passive abilities
		if (std::find(passiveIDs.begin(), passiveIDs.end(), static_cast<ItemSetPassiveAbilityID>(set->GetID())) != passiveIDs.end()) {
			return true;
		}
	}

	return false;
}

void InventoryComponent::DespawnPet() {
	auto* current = PetComponent::GetActivePet(m_Parent->GetObjectID());

	if (current != nullptr) {
		current->Deactivate();
	}
}

void InventoryComponent::SpawnPet(Item* item) {
	auto* current = PetComponent::GetActivePet(m_Parent->GetObjectID());

	if (current != nullptr) {
		current->Deactivate();

		if (current->GetDatabaseId() == item->GetSubKey()) {
			return;
		}
	}

	// First check if we can summon the pet.  You need 1 imagination to do so.
	auto destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();

	if (Game::config->GetValue("pets_take_imagination") == "1" && destroyableComponent && destroyableComponent->GetImagination() <= 0) {
		GameMessages::SendUseItemRequirementsResponse(m_Parent->GetObjectID(), m_Parent->GetSystemAddress(), UseItemResponse::NoImaginationForPet);
		return;
	}

	EntityInfo info{};
	info.lot = item->GetLot();
	info.pos = m_Parent->GetPosition();
	info.rot = NiQuaternion::IDENTITY;
	info.spawnerID = m_Parent->GetObjectID();

	auto* pet = EntityManager::Instance()->CreateEntity(info);

	auto* petComponent = pet->GetComponent<PetComponent>();

	if (petComponent != nullptr) {
		petComponent->Activate(item);
	}

	EntityManager::Instance()->ConstructEntity(pet);
}

void InventoryComponent::SetDatabasePet(LWOOBJID id, const DatabasePet& data) {
	m_Pets.insert_or_assign(id, data);
}

const DatabasePet& InventoryComponent::GetDatabasePet(LWOOBJID id) const {
	const auto& pair = m_Pets.find(id);

	if (pair == m_Pets.end()) return DATABASE_PET_INVALID;

	return pair->second;
}

bool InventoryComponent::IsPet(LWOOBJID id) const {
	const auto& pair = m_Pets.find(id);

	return pair != m_Pets.end();
}

void InventoryComponent::RemoveDatabasePet(LWOOBJID id) {
	m_Pets.erase(id);
}

BehaviorSlot InventoryComponent::FindBehaviorSlot(const eItemType type) {
	switch (type) {
	case eItemType::ITEM_TYPE_HAT:
		return BehaviorSlot::Head;
	case eItemType::ITEM_TYPE_NECK:
		return BehaviorSlot::Neck;
	case eItemType::ITEM_TYPE_LEFT_HAND:
		return BehaviorSlot::Offhand;
	case eItemType::ITEM_TYPE_RIGHT_HAND:
		return BehaviorSlot::Primary;
	case eItemType::ITEM_TYPE_CONSUMABLE:
		return BehaviorSlot::Consumable;
	default:
		return BehaviorSlot::Invalid;
	}
}

bool InventoryComponent::IsTransferInventory(eInventoryType type) {
	return type == VENDOR_BUYBACK || type == VAULT_ITEMS || type == VAULT_MODELS || type == TEMP_ITEMS || type == TEMP_MODELS;
}

uint32_t InventoryComponent::FindSkill(const LOT lot) {
	auto* table = CDClientManager::Instance()->GetTable<CDObjectSkillsTable>("ObjectSkills");

	const auto results = table->Query([=](const CDObjectSkills& entry) {
		return entry.objectTemplate == static_cast<unsigned int>(lot);
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
	if (item == nullptr) return buffs;
	auto* table = CDClientManager::Instance()->GetTable<CDObjectSkillsTable>("ObjectSkills");
	auto* behaviors = CDClientManager::Instance()->GetTable<CDSkillBehaviorTable>("SkillBehavior");

	const auto results = table->Query([=](const CDObjectSkills& entry) {
		return entry.objectTemplate == static_cast<unsigned int>(item->GetLot());
		});

	auto* missions = static_cast<MissionComponent*>(m_Parent->GetComponent(COMPONENT_TYPE_MISSION));

	for (const auto& result : results) {
		if (result.castOnType == 1) {
			const auto entry = behaviors->GetSkillByID(result.skillID);

			if (entry.skillID == 0) {
				Game::logger->Log("InventoryComponent", "Failed to find buff behavior for skill (%i)!", result.skillID);

				continue;
			}

			if (missions != nullptr && castOnEquip) {
				missions->Progress(MissionTaskType::MISSION_TASK_TYPE_SKILL, result.skillID);
			}

			// If item is not a proxy, add its buff to the added buffs.
			if (item->GetParent() == LWOOBJID_EMPTY) buffs.push_back(static_cast<uint32_t>(entry.behaviorID));
		}
	}

	return buffs;
}

void InventoryComponent::SetNPCItems(const std::vector<LOT>& items) {
	m_Equipped.clear();

	auto slot = 0u;

	for (const auto& item : items) {
		const LWOOBJID id = ObjectIDManager::Instance()->GenerateObjectID();

		const auto& info = Inventory::FindItemComponent(item);

		UpdateSlot(info.equipLocation, { id, static_cast<LOT>(item), 1, slot++ }, true);
	}

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

InventoryComponent::~InventoryComponent() {
	for (const auto& inventory : m_Inventories) {
		delete inventory.second;
	}

	m_Inventories.clear();

	for (auto* set : m_Itemsets) {
		delete set;
	}

	m_Itemsets.clear();
	m_Pets.clear();
}

std::vector<Item*> InventoryComponent::GenerateProxies(Item* parent) {
	std::vector<Item*> proxies;

	auto subItems = parent->GetInfo().subItems;

	if (subItems.empty()) {
		return proxies;
	}

	subItems.erase(std::remove_if(subItems.begin(), subItems.end(), ::isspace), subItems.end());

	std::stringstream stream(subItems);
	std::string segment;
	std::vector<uint32_t> lots;

	while (std::getline(stream, segment, ',')) {
		try {
			lots.push_back(std::stoi(segment));
		} catch (std::invalid_argument& exception) {
			Game::logger->Log("InventoryComponent", "Failed to parse proxy (%s): (%s)!", segment.c_str(), exception.what());
		}
	}

	for (const auto lot : lots) {
		if (!Inventory::IsValidItem(lot)) {
			continue;
		}

		auto* inventory = GetInventory(ITEM_SETS);

		auto* proxy = new Item(lot, inventory, inventory->FindEmptySlot(), 1, {}, parent->GetId(), false);

		EquipItem(proxy);

		proxies.push_back(proxy);
	}

	return proxies;
}

std::vector<Item*> InventoryComponent::FindProxies(const LWOOBJID parent) {
	auto* inventory = GetInventory(ITEM_SETS);

	std::vector<Item*> proxies;

	for (const auto& pair : inventory->GetItems()) {
		auto* item = pair.second;

		if (item->GetParent() == parent) {
			proxies.push_back(item);
		}
	}

	return proxies;
}

bool InventoryComponent::IsValidProxy(const LWOOBJID parent) {
	for (const auto& pair : m_Inventories) {
		const auto items = pair.second->GetItems();

		for (const auto& candidate : items) {
			auto* item = candidate.second;

			if (item->GetId() == parent) {
				return true;
			}
		}
	}

	return false;
}

bool InventoryComponent::IsParentValid(Item* root) {
	if (root->GetInfo().subItems.empty()) {
		return true;
	}

	const auto id = root->GetId();

	for (const auto& pair : m_Inventories) {
		const auto items = pair.second->GetItems();

		for (const auto& candidate : items) {
			auto* item = candidate.second;

			if (item->GetParent() == id) {
				return true;
			}
		}
	}

	return false;
}

void InventoryComponent::CheckProxyIntegrity() {
	std::vector<Item*> dead;

	for (const auto& pair : m_Inventories) {
		const auto& items = pair.second->GetItems();

		for (const auto& candidate : items) {
			auto* item = candidate.second;

			const auto parent = item->GetParent();

			if (parent == LWOOBJID_EMPTY) {
				continue;
			}

			if (IsValidProxy(parent)) {
				continue;
			}

			dead.push_back(item);
		}
	}

	for (auto* item : dead) {
		item->RemoveFromInventory();
	}

	dead.clear();

	/*
	for (const auto& pair : inventories)
	{
		const auto& items = pair.second->GetItems();

		for (const auto& candidate : items)
		{
			auto* item = candidate.second;

			const auto parent = item->GetParent();

			if (parent != LWOOBJID_EMPTY)
			{
				continue;
			}

			if (!item->IsEquipped())
			{
				continue;
			}

			if (IsParentValid(item))
			{
				continue;
			}

			dead.push_back(item);
		}
	}

	for (auto* item : dead)
	{
		item->RemoveFromInventory();
	}
	*/
}

void InventoryComponent::PurgeProxies(Item* item) {
	const auto root = item->GetParent();

	if (root != LWOOBJID_EMPTY) {
		item = FindItemById(root);

		if (item != nullptr) {
			UnEquipItem(item);
		}

		return;
	}

	auto proxies = FindProxies(item->GetId());

	for (auto* proxy : proxies) {
		proxy->UnEquip();

		proxy->RemoveFromInventory();
	}
}

void InventoryComponent::LoadPetXml(tinyxml2::XMLDocument* document) {
	auto* petInventoryElement = document->FirstChildElement("obj")->FirstChildElement("pet");

	if (petInventoryElement == nullptr) {
		m_Pets.clear();

		return;
	}

	auto* petElement = petInventoryElement->FirstChildElement();

	while (petElement != nullptr) {
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

	for (const auto& pet : m_Pets) {
		auto* petElement = document->NewElement("p");

		petElement->SetAttribute("id", pet.first);
		petElement->SetAttribute("l", pet.second.lot);
		petElement->SetAttribute("m", pet.second.moderationState);
		petElement->SetAttribute("n", pet.second.name.c_str());
		petElement->SetAttribute("t", 0);

		petInventoryElement->LinkEndChild(petElement);
	}
}
