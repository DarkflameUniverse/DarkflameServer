#include "Loot.h"

#include <algorithm>
#include <unordered_set>

#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "CDLootMatrixTable.h"
#include "CDLootTableTable.h"
#include "CDRarityTableTable.h"
#include "CDActivityRewardsTable.h"
#include "CDCurrencyTableTable.h"
#include "Character.h"
#include "Entity.h"
#include "GameMessages.h"
#include "GeneralUtils.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"
#include "eMissionState.h"
#include "eReplicaComponentType.h"

namespace {
	std::unordered_set<uint32_t> CachedMatrices;
}

void Loot::CacheMatrix(uint32_t matrixIndex) {
	if (CachedMatrices.find(matrixIndex) != CachedMatrices.end()) {
		return;
	}
	CachedMatrices.insert(matrixIndex);
	CDComponentsRegistryTable* componentsRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	CDItemComponentTable* itemComponentTable = CDClientManager::Instance().GetTable<CDItemComponentTable>();
	CDLootMatrixTable* lootMatrixTable = CDClientManager::Instance().GetTable<CDLootMatrixTable>();
	CDLootTableTable* lootTableTable = CDClientManager::Instance().GetTable<CDLootTableTable>();
	CDRarityTableTable* rarityTableTable = CDClientManager::Instance().GetTable<CDRarityTableTable>();

	const auto& matrix = lootMatrixTable->GetMatrix(matrixIndex);

	for (const auto& entry : matrix) {
		const auto& lootTable = lootTableTable->GetTable(entry.LootTableIndex);
		const auto& rarityTable = rarityTableTable->GetRarityTable(entry.RarityTableIndex);
		for (const auto& loot : lootTable) {
			uint32_t itemComponentId = componentsRegistryTable->GetByIDAndType(loot.itemid, eReplicaComponentType::ITEM);
			uint32_t rarity = itemComponentTable->GetItemComponentByID(itemComponentId).rarity;
		}
	}
}

std::unordered_map<LOT, int32_t> Loot::RollLootMatrix(Entity* player, uint32_t matrixIndex) {
	CDComponentsRegistryTable* componentsRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	CDItemComponentTable* itemComponentTable = CDClientManager::Instance().GetTable<CDItemComponentTable>();
	CDLootMatrixTable* lootMatrixTable = CDClientManager::Instance().GetTable<CDLootMatrixTable>();
	CDLootTableTable* lootTableTable = CDClientManager::Instance().GetTable<CDLootTableTable>();
	CDRarityTableTable* rarityTableTable = CDClientManager::Instance().GetTable<CDRarityTableTable>();
	auto* missionComponent = player->GetComponent<MissionComponent>();

	std::unordered_map<LOT, int32_t> drops;

	if (missionComponent == nullptr) return drops;

	const auto& matrix = lootMatrixTable->GetMatrix(matrixIndex);

	for (const auto& entry : matrix) {
		if (GeneralUtils::GenerateRandomNumber<float>(0, 1) < entry.percent) { // GetTable
			const auto& lootTable = lootTableTable->GetTable(entry.LootTableIndex);
			const auto& rarityTable = rarityTableTable->GetRarityTable(entry.RarityTableIndex);

			uint32_t dropCount = GeneralUtils::GenerateRandomNumber<uint32_t>(entry.minToDrop, entry.maxToDrop);
			for (uint32_t i = 0; i < dropCount; ++i) {
				uint32_t maxRarity = 1;

				float rarityRoll = GeneralUtils::GenerateRandomNumber<float>(0, 1);

				for (const auto& rarity : rarityTable) {
					if (rarity.randmax >= rarityRoll) {
						maxRarity = rarity.rarity;
					} else {
						break;
					}
				}

				bool rarityFound = false;
				std::vector<CDLootTable> possibleDrops;

				for (const auto& loot : lootTable) {
					uint32_t itemComponentId = componentsRegistryTable->GetByIDAndType(loot.itemid, eReplicaComponentType::ITEM);
					uint32_t rarity = itemComponentTable->GetItemComponentByID(itemComponentId).rarity;

					if (rarity == maxRarity) {
						possibleDrops.push_back(loot);
						rarityFound = true;
					} else if (rarity < maxRarity && !rarityFound) {
						possibleDrops.push_back(loot);
						maxRarity = rarity;
					}
				}

				if (possibleDrops.size() > 0) {
					const auto& drop = possibleDrops[GeneralUtils::GenerateRandomNumber<uint32_t>(0, possibleDrops.size() - 1)];

					// filter out uneeded mission items
					if (drop.MissionDrop && !missionComponent->RequiresItem(drop.itemid))
						continue;

					LOT itemID = drop.itemid;
					// convert faction token proxy
					if (itemID == 13763) {
						if (missionComponent->GetMissionState(545) == eMissionState::COMPLETE)
							itemID = 8318; // "Assembly Token"
						else if (missionComponent->GetMissionState(556) == eMissionState::COMPLETE)
							itemID = 8321; // "Venture League Token"
						else if (missionComponent->GetMissionState(567) == eMissionState::COMPLETE)
							itemID = 8319; // "Sentinels Token"
						else if (missionComponent->GetMissionState(578) == eMissionState::COMPLETE)
							itemID = 8320; // "Paradox Token"
					}

					if (itemID == 13763) {
						continue;
					} // check if we aren't in faction

					// drops[itemID]++; this should work?
					if (drops.find(itemID) == drops.end()) {
						drops.insert({ itemID, 1 });
					} else {
						++drops[itemID];
					}
				}
			}
		}
	}

	return drops;
}

std::unordered_map<LOT, int32_t> Loot::RollLootMatrix(uint32_t matrixIndex) {
	CDComponentsRegistryTable* componentsRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	CDItemComponentTable* itemComponentTable = CDClientManager::Instance().GetTable<CDItemComponentTable>();
	CDLootMatrixTable* lootMatrixTable = CDClientManager::Instance().GetTable<CDLootMatrixTable>();
	CDLootTableTable* lootTableTable = CDClientManager::Instance().GetTable<CDLootTableTable>();
	CDRarityTableTable* rarityTableTable = CDClientManager::Instance().GetTable<CDRarityTableTable>();
	std::unordered_map<LOT, int32_t> drops;

	const auto& matrix = lootMatrixTable->GetMatrix(matrixIndex);

	for (const auto& entry : matrix) {
		if (GeneralUtils::GenerateRandomNumber<float>(0, 1) < entry.percent) {
			const auto& lootTable = lootTableTable->GetTable(entry.LootTableIndex);
			const auto& rarityTable = rarityTableTable->GetRarityTable(entry.RarityTableIndex);

			uint32_t dropCount = GeneralUtils::GenerateRandomNumber<uint32_t>(entry.minToDrop, entry.maxToDrop);
			for (uint32_t i = 0; i < dropCount; ++i) {
				uint32_t maxRarity = 1;

				float rarityRoll = GeneralUtils::GenerateRandomNumber<float>(0, 1);

				for (const auto& rarity : rarityTable) {
					if (rarity.randmax >= rarityRoll) {
						maxRarity = rarity.rarity;
					} else {
						break;
					}
				}

				bool rarityFound = false;
				std::vector<CDLootTable> possibleDrops;

				for (const auto& loot : lootTable) {
					uint32_t itemComponentId = componentsRegistryTable->GetByIDAndType(loot.itemid, eReplicaComponentType::ITEM);
					uint32_t rarity = itemComponentTable->GetItemComponentByID(itemComponentId).rarity;

					if (rarity == maxRarity) {
						possibleDrops.push_back(loot);
						rarityFound = true;
					} else if (rarity < maxRarity && !rarityFound) {
						possibleDrops.push_back(loot);
						maxRarity = rarity;
					}
				}

				if (possibleDrops.size() > 0) {
					const auto& drop = possibleDrops[GeneralUtils::GenerateRandomNumber<uint32_t>(0, possibleDrops.size() - 1)];

					if (drops.find(drop.itemid) == drops.end()) {
						drops.insert({ drop.itemid, 1 });
					} else {
						++drops[drop.itemid];
					}
				}
			}
		}
	}

	return drops;
}

void Loot::GiveLoot(Entity* player, uint32_t matrixIndex, eLootSourceType lootSourceType) {
	player = player->GetOwner(); // If the owner is overwritten, we collect that here

	std::unordered_map<LOT, int32_t> result = RollLootMatrix(player, matrixIndex);

	GiveLoot(player, result, lootSourceType);
}

void Loot::GiveLoot(Entity* player, std::unordered_map<LOT, int32_t>& result, eLootSourceType lootSourceType) {
	player = player->GetOwner(); // if the owner is overwritten, we collect that here

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	if (!inventoryComponent)
		return;

	for (const auto& pair : result) {
		inventoryComponent->AddItem(pair.first, pair.second, lootSourceType);
	}
}

void Loot::GiveActivityLoot(Entity* player, Entity* source, uint32_t activityID, int32_t rating) {
	CDActivityRewardsTable* activityRewardsTable = CDClientManager::Instance().GetTable<CDActivityRewardsTable>();
	std::vector<CDActivityRewards> activityRewards = activityRewardsTable->Query([activityID](CDActivityRewards entry) { return (entry.objectTemplate == activityID); });

	const CDActivityRewards* selectedReward = nullptr;
	for (const auto& activityReward : activityRewards) {
		if (activityReward.activityRating <= rating && (selectedReward == nullptr || activityReward.activityRating > selectedReward->activityRating)) {
			selectedReward = &activityReward;
		}
	}

	if (!selectedReward)
		return;

	uint32_t minCoins = 0;
	uint32_t maxCoins = 0;

	CDCurrencyTableTable* currencyTableTable = CDClientManager::Instance().GetTable<CDCurrencyTableTable>();
	std::vector<CDCurrencyTable> currencyTable = currencyTableTable->Query([selectedReward](CDCurrencyTable entry) { return (entry.currencyIndex == selectedReward->CurrencyIndex && entry.npcminlevel == 1); });

	if (currencyTable.size() > 0) {
		minCoins = currencyTable[0].minvalue;
		maxCoins = currencyTable[0].maxvalue;
	}

	GiveLoot(player, selectedReward->LootMatrixIndex, eLootSourceType::ACTIVITY);

	uint32_t coins = static_cast<uint32_t>(minCoins + GeneralUtils::GenerateRandomNumber<float>(0, 1) * (maxCoins - minCoins));

	auto* character = player->GetCharacter();

	character->SetCoins(character->GetCoins() + coins, eLootSourceType::ACTIVITY);
}

void Loot::DropLoot(Entity* player, Entity* killedObject, uint32_t matrixIndex, uint32_t minCoins, uint32_t maxCoins) {
	player = player->GetOwner(); // if the owner is overwritten, we collect that here

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	if (!inventoryComponent)
		return;

	std::unordered_map<LOT, int32_t> result = RollLootMatrix(player, matrixIndex);

	DropLoot(player, killedObject, result, minCoins, maxCoins);
}

void Loot::DropLoot(Entity* player, Entity* killedObject, std::unordered_map<LOT, int32_t>& result, uint32_t minCoins, uint32_t maxCoins) {
	player = player->GetOwner(); // if the owner is overwritten, we collect that here

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	if (!inventoryComponent)
		return;

	const auto spawnPosition = killedObject->GetPosition();

	const auto source = killedObject->GetObjectID();

	for (const auto& pair : result) {
		for (int i = 0; i < pair.second; ++i) {
			GameMessages::SendDropClientLoot(player, source, pair.first, 0, spawnPosition, 1);
		}
	}

	uint32_t coins = static_cast<uint32_t>(minCoins + GeneralUtils::GenerateRandomNumber<float>(0, 1) * (maxCoins - minCoins));

	GameMessages::SendDropClientLoot(player, source, LOT_NULL, coins, spawnPosition);
}

void Loot::DropActivityLoot(Entity* player, Entity* source, uint32_t activityID, int32_t rating) {
	CDActivityRewardsTable* activityRewardsTable = CDClientManager::Instance().GetTable<CDActivityRewardsTable>();
	std::vector<CDActivityRewards> activityRewards = activityRewardsTable->Query([activityID](CDActivityRewards entry) { return (entry.objectTemplate == activityID); });

	const CDActivityRewards* selectedReward = nullptr;
	for (const auto& activityReward : activityRewards) {
		if (activityReward.activityRating <= rating && (selectedReward == nullptr || activityReward.activityRating > selectedReward->activityRating)) {
			selectedReward = &activityReward;
		}
	}

	if (selectedReward == nullptr) {
		return;
	}

	uint32_t minCoins = 0;
	uint32_t maxCoins = 0;

	CDCurrencyTableTable* currencyTableTable = CDClientManager::Instance().GetTable<CDCurrencyTableTable>();
	std::vector<CDCurrencyTable> currencyTable = currencyTableTable->Query([selectedReward](CDCurrencyTable entry) { return (entry.currencyIndex == selectedReward->CurrencyIndex && entry.npcminlevel == 1); });

	if (currencyTable.size() > 0) {
		minCoins = currencyTable[0].minvalue;
		maxCoins = currencyTable[0].maxvalue;
	}

	DropLoot(player, source, selectedReward->LootMatrixIndex, minCoins, maxCoins);
}
