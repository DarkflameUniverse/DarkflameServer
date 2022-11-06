#include <algorithm>

#include "Loot.h"

#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "CDLootMatrixTable.h"
#include "CDLootTableTable.h"
#include "CDRarityTableTable.h"
#include "Character.h"
#include "Entity.h"
#include "GameMessages.h"
#include "GeneralUtils.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"

LootGenerator::LootGenerator() {
	CDLootTableTable* lootTableTable = CDClientManager::Instance()->GetTable<CDLootTableTable>("LootTable");
	CDComponentsRegistryTable* componentsRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
	CDItemComponentTable* itemComponentTable = CDClientManager::Instance()->GetTable<CDItemComponentTable>("ItemComponent");
	CDLootMatrixTable* lootMatrixTable = CDClientManager::Instance()->GetTable<CDLootMatrixTable>("LootMatrix");
	CDRarityTableTable* rarityTableTable = CDClientManager::Instance()->GetTable<CDRarityTableTable>("RarityTable");

	// ==============================
	// Cache Item Rarities
	// ==============================

	std::vector<uint32_t> uniqueItems;

	for (const CDLootTable& loot : lootTableTable->GetEntries()) {
		uniqueItems.push_back(loot.itemid);
	}

	// filter out duplicates
	std::sort(uniqueItems.begin(), uniqueItems.end());
	uniqueItems.erase(std::unique(uniqueItems.begin(), uniqueItems.end()), uniqueItems.end());

	for (const uint32_t itemID : uniqueItems) {
		uint32_t itemComponentID = componentsRegistryTable->GetByIDAndType(itemID, COMPONENT_TYPE_ITEM);
		const CDItemComponent& item = itemComponentTable->GetItemComponentByID(itemComponentID);

		m_ItemRarities.insert({ itemID, item.rarity });
	}

	// ==============================
	// Cache Rarity Tables
	// ==============================

	std::vector<uint32_t> uniqueRarityIndices;

	for (const CDRarityTable& rarity : rarityTableTable->GetEntries()) {
		uniqueRarityIndices.push_back(rarity.RarityTableIndex);
	}

	// filter out duplicates
	std::sort(uniqueRarityIndices.begin(), uniqueRarityIndices.end());
	uniqueRarityIndices.erase(std::unique(uniqueRarityIndices.begin(), uniqueRarityIndices.end()), uniqueRarityIndices.end());

	for (const uint32_t index : uniqueRarityIndices) {
		std::vector<CDRarityTable> table = rarityTableTable->Query([index](const CDRarityTable& entry) { return entry.RarityTableIndex == index; });

		RarityTable rarityTable;

		for (const CDRarityTable& entry : table) {
			RarityTableEntry rarity{ entry.rarity, entry.randmax };
			rarityTable.push_back(rarity);
		}

		// sort in descending order based on randMax
		std::sort(rarityTable.begin(), rarityTable.end(), [](const RarityTableEntry& x, const RarityTableEntry& y) { return x.randMax > y.randMax; });

		m_RarityTables.insert({ index, rarityTable });
	}

	// ==============================
	// Cache Loot Matrices
	// ==============================

	std::vector<uint32_t> uniqueMatrixIndices;

	for (const CDLootMatrix& matrix : lootMatrixTable->GetEntries()) {
		uniqueMatrixIndices.push_back(matrix.LootMatrixIndex);
	}

	// filter out duplicates
	std::sort(uniqueMatrixIndices.begin(), uniqueMatrixIndices.end());
	uniqueMatrixIndices.erase(std::unique(uniqueMatrixIndices.begin(), uniqueMatrixIndices.end()), uniqueMatrixIndices.end());

	for (const uint32_t index : uniqueMatrixIndices) {
		std::vector<CDLootMatrix> matrix = lootMatrixTable->Query([index](const CDLootMatrix& entry) { return entry.LootMatrixIndex == index; });

		LootMatrix lootMatrix;

		for (const CDLootMatrix& entry : matrix) {
			LootMatrixEntry matrixEntry{ entry.LootTableIndex, entry.RarityTableIndex, entry.percent, entry.minToDrop, entry.maxToDrop };
			lootMatrix.push_back(matrixEntry);
		}

		m_LootMatrices.insert({ index, lootMatrix });
	}

	// ==============================
	// Cache Loot Tables
	// ==============================

	std::vector<uint32_t> uniqueTableIndices;

	for (const CDLootTable& entry : lootTableTable->GetEntries()) {
		uniqueTableIndices.push_back(entry.LootTableIndex);
	}

	// filter out duplicates
	std::sort(uniqueTableIndices.begin(), uniqueTableIndices.end());
	uniqueTableIndices.erase(std::unique(uniqueTableIndices.begin(), uniqueTableIndices.end()), uniqueTableIndices.end());

	for (const uint32_t index : uniqueTableIndices) {
		std::vector<CDLootTable> entries = lootTableTable->Query([index](const CDLootTable& entry) { return entry.LootTableIndex == index; });

		LootTable lootTable;

		for (const CDLootTable& entry : entries) {
			LootTableEntry tableEntry{ (LOT)entry.itemid, entry.MissionDrop };
			lootTable.push_back(tableEntry);
		}

		// sort by item rarity descending
		std::sort(lootTable.begin(), lootTable.end(), [&](const LootTableEntry& x, const LootTableEntry& y) {
			return m_ItemRarities[x.itemID] > m_ItemRarities[y.itemID];
			});

		m_LootTables.insert({ index, lootTable });
	}
}

std::unordered_map<LOT, int32_t> LootGenerator::RollLootMatrix(Entity* player, uint32_t matrixIndex) {
	auto* missionComponent = player->GetComponent<MissionComponent>();

	std::unordered_map<LOT, int32_t> drops;

	if (missionComponent == nullptr) {
		return drops;
	}

	const LootMatrix& matrix = m_LootMatrices[matrixIndex];

	for (const LootMatrixEntry& entry : matrix) {
		if (GeneralUtils::GenerateRandomNumber<float>(0, 1) < entry.percent) {
			const LootTable& lootTable = m_LootTables[entry.lootTableIndex];
			const RarityTable& rarityTable = m_RarityTables[entry.rarityTableIndex];

			uint32_t dropCount = GeneralUtils::GenerateRandomNumber<uint32_t>(entry.minDrop, entry.maxDrop);
			for (uint32_t i = 0; i < dropCount; ++i) {
				uint32_t maxRarity = 1;

				float rarityRoll = GeneralUtils::GenerateRandomNumber<float>(0, 1);

				for (const RarityTableEntry& rarity : rarityTable) {
					if (rarity.randMax >= rarityRoll) {
						maxRarity = rarity.rarity;
					} else {
						break;
					}
				}

				bool rarityFound = false;
				std::vector<LootTableEntry> possibleDrops;

				for (const LootTableEntry& loot : lootTable) {
					uint32_t rarity = m_ItemRarities[loot.itemID];

					if (rarity == maxRarity) {
						possibleDrops.push_back(loot);
						rarityFound = true;
					} else if (rarity < maxRarity && !rarityFound) {
						possibleDrops.push_back(loot);
						maxRarity = rarity;
					}
				}

				if (possibleDrops.size() > 0) {
					LootTableEntry drop = possibleDrops[GeneralUtils::GenerateRandomNumber<uint32_t>(0, possibleDrops.size() - 1)];

					// filter out uneeded mission items
					if (drop.isMissionDrop && !missionComponent->RequiresItem(drop.itemID))
						continue;

					// convert faction token proxy
					if (drop.itemID == 13763) {
						if (missionComponent->GetMissionState(545) == MissionState::MISSION_STATE_COMPLETE)
							drop.itemID = 8318; // "Assembly Token"
						else if (missionComponent->GetMissionState(556) == MissionState::MISSION_STATE_COMPLETE)
							drop.itemID = 8321; // "Venture League Token"
						else if (missionComponent->GetMissionState(567) == MissionState::MISSION_STATE_COMPLETE)
							drop.itemID = 8319; // "Sentinels Token"
						else if (missionComponent->GetMissionState(578) == MissionState::MISSION_STATE_COMPLETE)
							drop.itemID = 8320; // "Paradox Token"
					}

					if (drop.itemID == 13763) {
						continue;
					} // check if we aren't in faction

					if (drops.find(drop.itemID) == drops.end()) {
						drops.insert({ drop.itemID, 1 });
					} else {
						++drops[drop.itemID];
					}
				}
			}
		}
	}

	return drops;
}

std::unordered_map<LOT, int32_t> LootGenerator::RollLootMatrix(uint32_t matrixIndex) {
	std::unordered_map<LOT, int32_t> drops;

	const LootMatrix& matrix = m_LootMatrices[matrixIndex];

	for (const LootMatrixEntry& entry : matrix) {
		if (GeneralUtils::GenerateRandomNumber<float>(0, 1) < entry.percent) {
			const LootTable& lootTable = m_LootTables[entry.lootTableIndex];
			const RarityTable& rarityTable = m_RarityTables[entry.rarityTableIndex];

			uint32_t dropCount = GeneralUtils::GenerateRandomNumber<uint32_t>(entry.minDrop, entry.maxDrop);
			for (uint32_t i = 0; i < dropCount; ++i) {
				uint32_t maxRarity = 1;

				float rarityRoll = GeneralUtils::GenerateRandomNumber<float>(0, 1);

				for (const RarityTableEntry& rarity : rarityTable) {
					if (rarity.randMax >= rarityRoll) {
						maxRarity = rarity.rarity;
					} else {
						break;
					}
				}

				bool rarityFound = false;
				std::vector<LootTableEntry> possibleDrops;

				for (const LootTableEntry& loot : lootTable) {
					uint32_t rarity = m_ItemRarities[loot.itemID];

					if (rarity == maxRarity) {
						possibleDrops.push_back(loot);
						rarityFound = true;
					} else if (rarity < maxRarity && !rarityFound) {
						possibleDrops.push_back(loot);
						maxRarity = rarity;
					}
				}

				if (possibleDrops.size() > 0) {
					const LootTableEntry& drop = possibleDrops[GeneralUtils::GenerateRandomNumber<uint32_t>(0, possibleDrops.size() - 1)];

					if (drops.find(drop.itemID) == drops.end()) {
						drops.insert({ drop.itemID, 1 });
					} else {
						++drops[drop.itemID];
					}
				}
			}
		}
	}

	return drops;
}

void LootGenerator::GiveLoot(Entity* player, uint32_t matrixIndex, eLootSourceType lootSourceType) {
	player = player->GetOwner(); // If the owner is overwritten, we collect that here

	std::unordered_map<LOT, int32_t> result = RollLootMatrix(player, matrixIndex);

	GiveLoot(player, result, lootSourceType);
}

void LootGenerator::GiveLoot(Entity* player, std::unordered_map<LOT, int32_t>& result, eLootSourceType lootSourceType) {
	player = player->GetOwner(); // if the owner is overwritten, we collect that here

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	if (!inventoryComponent)
		return;

	for (const auto& pair : result) {
		inventoryComponent->AddItem(pair.first, pair.second, lootSourceType);
	}
}

void LootGenerator::GiveActivityLoot(Entity* player, Entity* source, uint32_t activityID, int32_t rating) {
	CDActivityRewardsTable* activityRewardsTable = CDClientManager::Instance()->GetTable<CDActivityRewardsTable>("ActivityRewards");
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

	CDCurrencyTableTable* currencyTableTable = CDClientManager::Instance()->GetTable<CDCurrencyTableTable>("CurrencyTable");
	std::vector<CDCurrencyTable> currencyTable = currencyTableTable->Query([selectedReward](CDCurrencyTable entry) { return (entry.currencyIndex == selectedReward->CurrencyIndex && entry.npcminlevel == 1); });

	if (currencyTable.size() > 0) {
		minCoins = currencyTable[0].minvalue;
		maxCoins = currencyTable[0].maxvalue;
	}

	GiveLoot(player, selectedReward->LootMatrixIndex, eLootSourceType::LOOT_SOURCE_ACTIVITY);

	uint32_t coins = (int)(minCoins + GeneralUtils::GenerateRandomNumber<float>(0, 1) * (maxCoins - minCoins));

	auto* character = player->GetCharacter();

	character->SetCoins(character->GetCoins() + coins, eLootSourceType::LOOT_SOURCE_ACTIVITY);
}

void LootGenerator::DropLoot(Entity* player, Entity* killedObject, uint32_t matrixIndex, uint32_t minCoins, uint32_t maxCoins) {
	player = player->GetOwner(); // if the owner is overwritten, we collect that here

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	if (!inventoryComponent)
		return;

	std::unordered_map<LOT, int32_t> result = RollLootMatrix(player, matrixIndex);

	DropLoot(player, killedObject, result, minCoins, maxCoins);
}

void LootGenerator::DropLoot(Entity* player, Entity* killedObject, std::unordered_map<LOT, int32_t>& result, uint32_t minCoins, uint32_t maxCoins) {
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

	uint32_t coins = (int)(minCoins + GeneralUtils::GenerateRandomNumber<float>(0, 1) * (maxCoins - minCoins));

	GameMessages::SendDropClientLoot(player, source, LOT_NULL, coins, spawnPosition);
}

void LootGenerator::DropActivityLoot(Entity* player, Entity* source, uint32_t activityID, int32_t rating) {
	CDActivityRewardsTable* activityRewardsTable = CDClientManager::Instance()->GetTable<CDActivityRewardsTable>("ActivityRewards");
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

	CDCurrencyTableTable* currencyTableTable = CDClientManager::Instance()->GetTable<CDCurrencyTableTable>("CurrencyTable");
	std::vector<CDCurrencyTable> currencyTable = currencyTableTable->Query([selectedReward](CDCurrencyTable entry) { return (entry.currencyIndex == selectedReward->CurrencyIndex && entry.npcminlevel == 1); });

	if (currencyTable.size() > 0) {
		minCoins = currencyTable[0].minvalue;
		maxCoins = currencyTable[0].maxvalue;
	}

	DropLoot(player, source, selectedReward->LootMatrixIndex, minCoins, maxCoins);
}
