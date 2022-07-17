#include "Inventory.h"
#include "GameMessages.h"
#include "Game.h"
#include "Item.h"
#include "eItemType.h"

std::vector<LOT> Inventory::m_GameMasterRestrictedItems = {
		1727, // GM Only - JetPack
		2243, // GM Only - Hammer of Doom
		3293, // GM Only - Flamethrower
		3735, // GM Only - Large Jetpack
		5873, // GM Only - Winged Helm of Speed
		6407, // Gm Only - Hat of Pwnage
		14442 // The jamesster jetpack
};

Inventory::Inventory(const eInventoryType type, const uint32_t size, const std::vector<Item*>& items, InventoryComponent* component)
{
	this->type = type;
	this->size = size;
	this->free = size;
	this->component = component;

	for (auto* item : items)
	{
		AddManagedItem(item);
	}
}

eInventoryType Inventory::GetType() const
{
	return type;
}

uint32_t Inventory::GetSize() const
{
	return size;
}

std::map<LWOOBJID, Item*>& Inventory::GetItems()
{
	return items;
}

std::map<uint32_t, Item*> Inventory::GetSlots() const
{
	std::map<uint32_t, Item*> slots;

	for (const auto& pair : items)
	{
		auto* item = pair.second;

		slots.insert_or_assign(item->GetSlot(), item);
	}

	return slots;
}

InventoryComponent* Inventory::GetComponent() const
{
	return component;
}

uint32_t Inventory::GetLotCount(const LOT lot) const
{
	uint32_t count = 0;

	for (const auto& pair : items)
	{
		const auto* item = pair.second;

		if (item->GetLot() == lot)
		{
			count += item->GetCount();
		}
	}

	return count;
}

void Inventory::SetSize(const uint32_t value)
{
	free += static_cast<int32_t>(value) - static_cast<int32_t>(size);
	
	size = value;

	GameMessages::SendSetInventorySize(component->GetParent(), type, static_cast<int>(size));
}

int32_t Inventory::FindEmptySlot()
{
	if (free <= 6) // Up from 1
	{
		if (type != ITEMS && type != VAULT_ITEMS && type != eInventoryType::VAULT_MODELS)
		{
			uint32_t newSize = size;

			if (type == MODELS)
			{
				newSize = 240;
			}
			else if (type == eInventoryType::VENDOR_BUYBACK)
			{
				newSize += 9u;
			}
			else
			{
				newSize += 10u;
			}
			
			if (newSize > GetSize())
			{
				SetSize(newSize);
			}
		}
	}

	if (free == 0)
	{
		return -1;
	}

	const auto slots = GetSlots();
	
	for (auto i = 0u; i < size; ++i)
	{
		if (slots.find(i) == slots.end())
		{
			return i;
		}
	}

	return -1;
}

int32_t Inventory::GetEmptySlots() 
{
	return free;
}

bool Inventory::IsSlotEmpty(int32_t slot) 
{
	const auto slots = GetSlots();
	
	const auto& index = slots.find(slot);

	return index == slots.end();
}

Item* Inventory::FindItemById(const LWOOBJID id) const
{
	const auto& index = items.find(id);

	if (index == items.end())
	{
		return nullptr;
	}

	return index->second;
}

Item* Inventory::FindItemByLot(const LOT lot, const bool ignoreEquipped, const bool ignoreBound) const
{
	Item* smallest = nullptr;

	for (const auto& pair : items)
	{
		auto* item = pair.second;

		if (item->GetLot() != lot)
		{
			continue;
		}

		if (ignoreEquipped && item->IsEquipped())
		{
			continue;
		}

		if (ignoreBound && item->GetBound())
		{
			continue;
		}

		if (smallest == nullptr)
		{
			smallest = item;

			continue;
		}

		if (smallest->GetCount() > item->GetCount())
		{
			smallest = item;
		}
	}

	return smallest;
}

Item* Inventory::FindItemBySlot(const uint32_t slot) const
{
	const auto slots = GetSlots();
	
	const auto index = slots.find(slot);

	if (index == slots.end())
	{
		return nullptr;
	}

	return index->second;
}

Item* Inventory::FindItemBySubKey(LWOOBJID id) const
{
	for (const auto& item : items)
	{
		if (item.second->GetSubKey() == id)
		{
			return item.second;
		}
	}

	return nullptr;
}

void Inventory::AddManagedItem(Item* item)
{
	const auto id = item->GetId();
	
	if (items.find(id) != items.end())
	{
		Game::logger->Log("Inventory", "Attempting to add an item with an already present id (%llu)!\n", id);

		return;
	}
	
	const auto slots = GetSlots();

	const auto slot = item->GetSlot();

	if (slots.find(slot) != slots.end())
	{
		Game::logger->Log("Inventory", "Attempting to add an item with an already present slot (%i)!\n", slot);

		return;
	}

	items.insert_or_assign(id, item);

	free--;
}

void Inventory::RemoveManagedItem(Item* item)
{
	const auto id = item->GetId();

	if (items.find(id) == items.end())
	{
		Game::logger->Log("Inventory", "Attempting to remove an item with an invalid id (%llu), lot (%i)!\n", id, item->GetLot());

		return;
	}

	items.erase(id);

	free++;
}

eInventoryType Inventory::FindInventoryTypeForLot(const LOT lot)
{
	auto itemComponent = FindItemComponent(lot);

	const auto itemType = static_cast<eItemType>(itemComponent.itemType);

	switch (itemType) {
	case eItemType::ITEM_TYPE_BRICK:
		return BRICKS;
		
	case eItemType::ITEM_TYPE_BEHAVIOR:
		return BEHAVIORS;

	case eItemType::ITEM_TYPE_PROPERTY:
		return PROPERTY_DEEDS;

	case eItemType::ITEM_TYPE_MODEL:
	case eItemType::ITEM_TYPE_VEHICLE:
	case eItemType::ITEM_TYPE_LOOT_MODEL:
	case eItemType::ITEM_TYPE_MOUNT:
		return MODELS;
		
	case eItemType::ITEM_TYPE_HAT:
	case eItemType::ITEM_TYPE_HAIR:
	case eItemType::ITEM_TYPE_NECK:
	case eItemType::ITEM_TYPE_LEFT_HAND:
	case eItemType::ITEM_TYPE_RIGHT_HAND:
	case eItemType::ITEM_TYPE_LEGS:
	case eItemType::ITEM_TYPE_LEFT_TRINKET:
	case eItemType::ITEM_TYPE_RIGHT_TRINKET:
	case eItemType::ITEM_TYPE_COLLECTIBLE:
	case eItemType::ITEM_TYPE_CONSUMABLE:
	case eItemType::ITEM_TYPE_CHEST:
	case eItemType::ITEM_TYPE_EGG:
	case eItemType::ITEM_TYPE_PET_FOOD:
	case eItemType::ITEM_TYPE_PET_INVENTORY_ITEM:
	case eItemType::ITEM_TYPE_PACKAGE:
	case eItemType::ITEM_TYPE_CURRENCY:
		return ITEMS;
	
	case eItemType::ITEM_TYPE_QUEST_OBJECT:
	case eItemType::ITEM_TYPE_UNKNOWN:
	default:
		return HIDDEN;
	}
}

const CDItemComponent& Inventory::FindItemComponent(const LOT lot)
{
	auto* registry = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");

	auto* itemComponents = CDClientManager::Instance()->GetTable<CDItemComponentTable>("ItemComponent");

	const auto componentId = registry->GetByIDAndType(lot, COMPONENT_TYPE_ITEM);

	if (componentId == 0)
	{
		Game::logger->Log("Inventory", "Failed to find item component for (%i)!\n", lot);

		return CDItemComponentTable::Default;
	}
	
	const auto& itemComponent = itemComponents->GetItemComponentByID(componentId);

	return itemComponent;
}

bool Inventory::IsValidItem(const LOT lot)
{
	auto* registry = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");

	const auto componentId = registry->GetByIDAndType(lot, COMPONENT_TYPE_ITEM);

	return componentId != 0;
}

const std::vector<LOT>& Inventory::GetAllGMItems() 
{
	return m_GameMasterRestrictedItems;
}

Inventory::~Inventory()
{
	for (auto item : items)
	{
		delete item.second;
	}

	items.clear();
}
