#include "Loot.h"
#include "GameMessages.h"

#include "CDClientManager.h"
#include "CDLootMatrixTable.h"
#include "CDLootTableTable.h"

#include "SimplePhysicsComponent.h"
#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "MissionComponent.h"
#include "CharacterComponent.h"
#include "TeamManager.h"

#include <algorithm>

std::vector<CDLootTable> Loot::GetLootOfRarity(const std::vector<CDLootTable> &lootTable, uint32_t rarity) {
	std::vector<CDLootTable> refinedLoot;
	for (auto loot : lootTable) {
		CDItemComponent item = Inventory::FindItemComponent(loot.itemid);
		if (item.rarity == rarity) {
			refinedLoot.push_back(loot);
		}
		else if (item.rarity == 0) {
			refinedLoot.push_back(loot); // powerups
		}
	}

	return refinedLoot;
}

void Loot::GiveLoot(Entity* user, uint32_t lootMatrixID) {
	user = user->GetOwner(); // If the owner is overwritten, we collect that here

	std::unordered_map<LOT, int32_t> result {};

	CalculateLootMatrix(lootMatrixID, user, result);

	GiveLoot(user, result);
}

void Loot::DropLoot(Entity* user, Entity* killedObject, uint32_t lootMatrixID, uint32_t minCoins, uint32_t maxCoins) {
	user = user->GetOwner(); // If the owner is overwritten, we collect that here

	auto* inventoryComponent = user->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	std::unordered_map<LOT, int32_t> result {};

	CalculateLootMatrix(lootMatrixID, user, result);

	DropLoot(user, killedObject, result, minCoins, maxCoins);
}

void Loot::GiveLoot(Entity* user, std::unordered_map<LOT, int32_t>& result) {
	user = user->GetOwner(); // If the owner is overwritten, we collect that here

	auto* inventoryComponent = user->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	for (const auto& pair : result) {
		inventoryComponent->AddItem(pair.first, pair.second);
	}
}

void Loot::DropLoot(Entity* user, Entity* killedObject, std::unordered_map<LOT, int32_t>& result, uint32_t minCoins, uint32_t maxCoins) {
	user = user->GetOwner(); // If the owner is overwritten, we collect that here

	auto* inventoryComponent = user->GetComponent<InventoryComponent>();

	if (inventoryComponent == nullptr) {
		return;
	}

	const auto spawnPosition = killedObject->GetPosition();
	
	const auto source = killedObject->GetObjectID();

	for (const auto& pair : result) {
		for (int i = 0; i < pair.second; ++i) {
			GameMessages::SendDropClientLoot(user, source, pair.first, 0, spawnPosition, 1);
		}
	}

	uint32_t coins = (int)(minCoins + GeneralUtils::GenerateRandomNumber<float>(0, 1) * (maxCoins - minCoins));

	GameMessages::SendDropClientLoot(user, source, LOT_NULL, coins, spawnPosition);
}

void Loot::DropActivityLoot(Entity* user, Entity* source, uint32_t activityID, int32_t rating)
{
	CDActivityRewardsTable* activityRewardsTable = CDClientManager::Instance()->GetTable<CDActivityRewardsTable>("ActivityRewards");
	std::vector<CDActivityRewards> activityRewards = activityRewardsTable->Query([=](CDActivityRewards entry) { return (entry.objectTemplate == activityID); });

	const CDActivityRewards* selectedReward = nullptr;
	for (const auto& activityReward : activityRewards)
	{
		if (activityReward.activityRating <= rating && (selectedReward == nullptr || activityReward.activityRating > selectedReward->activityRating))
		{
			selectedReward = &activityReward;
		}
	}

	if (selectedReward == nullptr)
	{
		return;
	}

	uint32_t minCoins = 0;
	uint32_t maxCoins = 0;

	CDCurrencyTableTable* currencyTableTable = CDClientManager::Instance()->GetTable<CDCurrencyTableTable>("CurrencyTable");
	std::vector<CDCurrencyTable> currencyTable = currencyTableTable->Query([=](CDCurrencyTable entry) { return (entry.currencyIndex == selectedReward->CurrencyIndex && entry.npcminlevel == 1); });

	if (currencyTable.size() > 0) {
		minCoins = currencyTable[0].minvalue;
		maxCoins = currencyTable[0].maxvalue;
	}

	Loot::DropLoot(user, source, selectedReward->LootMatrixIndex, minCoins, maxCoins);
}

void Loot::GiveActivityLoot(Entity* user, Entity* source, uint32_t activityID, int32_t rating) 
{
	CDActivityRewardsTable* activityRewardsTable = CDClientManager::Instance()->GetTable<CDActivityRewardsTable>("ActivityRewards");
	std::vector<CDActivityRewards> activityRewards = activityRewardsTable->Query([=](CDActivityRewards entry) { return (entry.objectTemplate == activityID); });

	const CDActivityRewards* selectedReward = nullptr;
	for (const auto& activityReward : activityRewards)
	{
		if (activityReward.activityRating <= rating && (selectedReward == nullptr || activityReward.activityRating > selectedReward->activityRating))
		{
			selectedReward = &activityReward;
		}
	}

	if (selectedReward == nullptr)
	{
		return;
	}

	uint32_t minCoins = 0;
	uint32_t maxCoins = 0;

	CDCurrencyTableTable* currencyTableTable = CDClientManager::Instance()->GetTable<CDCurrencyTableTable>("CurrencyTable");
	std::vector<CDCurrencyTable> currencyTable = currencyTableTable->Query([=](CDCurrencyTable entry) { return (entry.currencyIndex == selectedReward->CurrencyIndex && entry.npcminlevel == 1); });

	if (currencyTable.size() > 0) {
		minCoins = currencyTable[0].minvalue;
		maxCoins = currencyTable[0].maxvalue;
	}

	Loot::GiveLoot(user, selectedReward->LootMatrixIndex);

	uint32_t coins = (int)(minCoins + GeneralUtils::GenerateRandomNumber<float>(0, 1) * (maxCoins - minCoins));

	auto* charactert = user->GetCharacter();

	charactert->SetCoins(charactert->GetCoins() + coins);
}

void Loot::CalculateLootMatrix(uint32_t lootMatrixID, Entity* user, std::unordered_map<LOT, int32_t>& result) 
{
	user = user->GetOwner();

	auto* missionComponent = user->GetComponent<MissionComponent>();

	// Get our loot for this LOT's lootMatrixID:
	CDLootMatrixTable* lootMatrixTable = CDClientManager::Instance()->GetTable<CDLootMatrixTable>("LootMatrix");
	CDLootTableTable* lootTableTable = CDClientManager::Instance()->GetTable<CDLootTableTable>("LootTable");
	CDRarityTableTable* rarityTableTable = CDClientManager::Instance()->GetTable<CDRarityTableTable>("RarityTable");

	std::vector<CDLootMatrix> lootMatrix = lootMatrixTable->Query([lootMatrixID](CDLootMatrix entry) { return (entry.LootMatrixIndex == lootMatrixID); });

	// Now, loop through each entry
	for (uint32_t i = 0; i < lootMatrix.size(); ++i) {
		// Now, determine whether or not we should drop this
		float chanceToDrop = 1.0 - lootMatrix[i].percent;
		float shouldDrop = GeneralUtils::GenerateRandomNumber<float>(0, 1);

		const auto rarityTableIndex = lootMatrix[i].RarityTableIndex;

		std::vector<CDRarityTable> rarityTable = rarityTableTable->Query([rarityTableIndex](CDRarityTable entry) { return (entry.RarityTableIndex == rarityTableIndex); });
		
		std::sort(rarityTable.begin(), rarityTable.end());

		if (shouldDrop < chanceToDrop) {
			// We are not able to drop this item, so continue
			continue;
		}

		// If we reached here, we are able to drop the item
		uint32_t minToDrop = lootMatrix[i].minToDrop;
		uint32_t maxToDrop = lootMatrix[i].maxToDrop;

		// Now determine the number we will drop of items from this table
		uint32_t numToDrop = GeneralUtils::GenerateRandomNumber<uint32_t>(minToDrop, maxToDrop);

		// Now, query the loot matrix index
		const auto lootTableIndex = lootMatrix[i].LootTableIndex;

		std::vector<CDLootTable> lootTable = lootTableTable->Query([lootTableIndex](CDLootTable entry) { return (entry.LootTableIndex == lootTableIndex); });

		// Now randomize these entries
		if (lootTable.size() > 1) {
			std::shuffle(std::begin(lootTable), std::end(lootTable), Game::randomEngine);
		}

		uint32_t addedItems = 0;

		if (lootTable.empty()) continue;

		while (addedItems < numToDrop) {
			addedItems++;
			
			float rarityRoll = GeneralUtils::GenerateRandomNumber<float>(0, 1);

			// im sorry
			uint32_t highestRarity = 1; // LOOT_COMMON
			float highestRandMax = 0.0f;
			for (const auto& rarity : rarityTable) {
				if (rarityRoll > rarity.randmax && rarity.randmax > highestRandMax) {
					highestRandMax = rarity.randmax;
					highestRarity = rarity.rarity + 1;
				}
			}

			std::vector<CDLootTable> refinedLoot;

			if (lootTable.size() == 1)
			{
				refinedLoot = lootTable;
			}
			else
			{
				refinedLoot = GetLootOfRarity(lootTable, highestRarity);

				bool continueLoop = false;
				while (refinedLoot.empty())
				{
					if (highestRarity == 1)
					{
						continueLoop = true;
						break;
					}

					highestRarity -= 1;

					refinedLoot = GetLootOfRarity(lootTable, highestRarity);

					if (!refinedLoot.empty())
					{
						break;
					}
				}

				if (continueLoop) continue;
			}
			
			int randomTable = GeneralUtils::GenerateRandomNumber<int>(0, refinedLoot.size() - 1);

			const auto& selectedTable = refinedLoot[randomTable];

			uint32_t itemLOT = selectedTable.itemid;
			bool isMissionItem = selectedTable.MissionDrop;

			if (isMissionItem && missionComponent != nullptr)
			{
                // TODO: this executes a query in a hot path, might be worth refactoring away
				if (!missionComponent->RequiresItem(itemLOT))
				{
					continue;
				}
			}

			if (lootTable.size() > numToDrop)
			{
				for (size_t i = 0; i < lootTable.size(); i++)
				{
					if (lootTable[i].id == selectedTable.id)
					{
						lootTable.erase(lootTable.begin() + i);

						break;
					}
				}
			}

			const auto& it = result.find(itemLOT);
			if (it != result.end()) {
				it->second++;
			}
			else {
				result.emplace(itemLOT, 1);
			}
		}
	}

	int32_t tokenCount = 0;

	const auto& tokens = result.find(13763);

	if (tokens != result.end()) {
		tokenCount = tokens->second;

		result.erase(tokens);
	}

	if (tokenCount == 0 || user == nullptr) {
		return;
	}

	if (missionComponent == nullptr) {
		return;
	}

	LOT tokenId = -1;

	if (missionComponent->GetMissionState(545) == MissionState::MISSION_STATE_COMPLETE) // "Join Assembly!"
	{
		tokenId = 8318; // "Assembly Token"
	}

	if (missionComponent->GetMissionState(556) == MissionState::MISSION_STATE_COMPLETE) // "Join Venture League!"
	{
		tokenId = 8321; // "Venture League Token"
	}

	if (missionComponent->GetMissionState(567) == MissionState::MISSION_STATE_COMPLETE) // "Join The Sentinels!"
	{
		tokenId = 8319; // "Sentinels Token"
	}

	if (missionComponent->GetMissionState(578) == MissionState::MISSION_STATE_COMPLETE) // "Join Paradox!"
	{
		tokenId = 8320; // "Paradox Token"
	}

	if (tokenId != -1)
	{
		result.emplace(tokenId, tokenCount);
	}
}

void Loot::DropItem(Entity* user, Entity* sourceObject, LOT item, int32_t currency, int32_t count, bool useTeam, bool freeForAll) 
{
	if (sourceObject == nullptr)
	{
		return;
	}

	const auto sourceID = sourceObject->GetObjectID();
	const auto sourcePosition = sourceObject->GetPosition();

	// If useTeam, drop the item once for each team member.
	auto* team = TeamManager::Instance()->GetTeam(user->GetObjectID());

	if (team != nullptr && useTeam)
	{
		for (const auto& memberID : team->members)
		{
			// Get the team member from its ID.
			auto* member = EntityManager::Instance()->GetEntity(memberID);

			if (member == nullptr) 
			{
				continue;
			}

			// Drop the item.
			GameMessages::SendDropClientLoot(member, sourceID, item, currency, sourcePosition, count);
		}

		return;
	}

	GameMessages::SendDropClientLoot(user, sourceID, item, currency, sourcePosition, count);
}
