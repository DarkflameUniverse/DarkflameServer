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
#include "TeamManager.h"
#include "CDObjectsTable.h"
#include "ObjectIDManager.h"

namespace {
	std::unordered_set<uint32_t> CachedMatrices;
	constexpr float g_MAX_DROP_RADIUS = 700.0f;
}

struct LootDropInfo {
	CDLootTable table{};
	uint32_t count{ 0 };
};

std::map<LOT, LootDropInfo> RollLootMatrix(uint32_t matrixIndex) {
	CDComponentsRegistryTable* componentsRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	CDItemComponentTable* itemComponentTable = CDClientManager::GetTable<CDItemComponentTable>();
	CDLootMatrixTable* lootMatrixTable = CDClientManager::GetTable<CDLootMatrixTable>();
	CDLootTableTable* lootTableTable = CDClientManager::GetTable<CDLootTableTable>();
	CDRarityTableTable* rarityTableTable = CDClientManager::GetTable<CDRarityTableTable>();
	std::map<LOT, LootDropInfo> drops;

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

				if (!possibleDrops.empty()) {
					const auto& drop = possibleDrops[GeneralUtils::GenerateRandomNumber<uint32_t>(0, possibleDrops.size() - 1)];

					auto& info = drops[drop.itemid];
					if (info.count == 0) info.table = drop;
					info.count++;
				}
			}
		}
	}

	return drops;
}

// Generates a 'random' final position for the loot drop based on its input spawn position.
void CalcFinalDropPos(GameMessages::DropClientLoot& lootMsg) {
	if (lootMsg.spawnPos != NiPoint3Constant::ZERO) {
		lootMsg.bUsePosition = true;

		//Calculate where the loot will go:
		uint16_t degree = GeneralUtils::GenerateRandomNumber<uint16_t>(0, 360);

		double rad = degree * 3.14 / 180;
		double sin_v = sin(rad) * 4.2;
		double cos_v = cos(rad) * 4.2;

		const auto [x, y, z] = lootMsg.spawnPos;
		lootMsg.finalPosition = NiPoint3(static_cast<float>(x + sin_v), y, static_cast<float>(z + cos_v));
	}
}

// Visually drop the loot to all team members, though only the lootMsg.ownerID can pick it up
void DistrbuteMsgToTeam(const GameMessages::DropClientLoot& lootMsg, const Team& team) {
	for (const auto memberClient : team.members) {
		const auto* const memberEntity = Game::entityManager->GetEntity(memberClient);
		if (memberEntity) lootMsg.Send(memberEntity->GetSystemAddress());
	}
}

// The following 8 functions are all ever so slightly different such that combining them
// would make the logic harder to follow. Please read the comments!

// Given a faction token proxy LOT to drop, drop 1 token for each player on a team, or the provided player.
// token drops are always given to every player on the team.
void DropFactionLoot(Entity& player, GameMessages::DropClientLoot& lootMsg) {
	const auto playerID = player.GetObjectID();

	GameMessages::GetFactionTokenType factionTokenType{};
	factionTokenType.target = playerID;
	// If we're not in a faction, this message will return false
	if (factionTokenType.Send()) {
		lootMsg.item = factionTokenType.tokenType;
		lootMsg.target = playerID;
		lootMsg.ownerID = playerID;
		lootMsg.lootID = ObjectIDManager::GenerateObjectID();
		CalcFinalDropPos(lootMsg);
		// Register the drop on the player
		lootMsg.Send();
		// Visually drop it for the player
		lootMsg.Send(player.GetSystemAddress());
	}
}

// Drops 1 token for each player on a team
// token drops are always given to every player on the team.
void DropFactionLoot(const Team& team, GameMessages::DropClientLoot& lootMsg) {
	for (const auto member : team.members) {
		GameMessages::GetPosition memberPosMsg{};
		memberPosMsg.target = member;
		memberPosMsg.Send();
		if (NiPoint3::Distance(memberPosMsg.pos, lootMsg.spawnPos) > g_MAX_DROP_RADIUS) continue;

		GameMessages::GetFactionTokenType factionTokenType{};
		factionTokenType.target = member;
		// If we're not in a faction, this message will return false
		if (factionTokenType.Send()) {
			lootMsg.item = factionTokenType.tokenType;
			lootMsg.target = member;
			lootMsg.ownerID = member;
			lootMsg.lootID = ObjectIDManager::GenerateObjectID();
			CalcFinalDropPos(lootMsg);
			// Register the drop on this team member
			lootMsg.Send();
			// Show the rewards on all connected members of the team. Only the loot owner will be able to pick the tokens up.
			DistrbuteMsgToTeam(lootMsg, team);
		}
	}
}

// Drop the power up with no owner
// Power ups can be picked up by anyone on a team, however unlike actual loot items,
// if multiple clients say they picked one up, we let them pick it up.
void DropPowerupLoot(Entity& player, GameMessages::DropClientLoot& lootMsg) {
	const auto playerID = player.GetObjectID();
	CalcFinalDropPos(lootMsg);

	lootMsg.lootID = ObjectIDManager::GenerateObjectID();
	lootMsg.ownerID = playerID;
	lootMsg.target = playerID;

	// Register the drop on the player
	lootMsg.Send();
	// Visually drop it for the player
	lootMsg.Send(player.GetSystemAddress());
}

// Drop the power up with no owner
// Power ups can be picked up by anyone on a team, however unlike actual loot items,
// if multiple clients say they picked one up, we let them pick it up.
void DropPowerupLoot(const Team& team, GameMessages::DropClientLoot& lootMsg) {
	lootMsg.lootID = ObjectIDManager::GenerateObjectID();
	lootMsg.ownerID = LWOOBJID_EMPTY; // By setting ownerID to empty, any client that gets this DropClientLoot message can pick up the item.
	CalcFinalDropPos(lootMsg);

	// We want to drop the powerups as the same ID and the same position to all members of the team
	for (const auto member : team.members) {
		GameMessages::GetPosition memberPosMsg{};
		memberPosMsg.target = member;
		memberPosMsg.Send();
		if (NiPoint3::Distance(memberPosMsg.pos, lootMsg.spawnPos) > g_MAX_DROP_RADIUS) continue;

		lootMsg.target = member;
		// By sending this message with the same ID to all players on the team, all players on the team are allowed to pick it up.
		lootMsg.Send();
		// No need to send to all members in a loop since that will happen by using the outer loop above and also since there is no owner
		// sending to all will do nothing.
		const auto* const memberEntity = Game::entityManager->GetEntity(member);
		if (memberEntity) lootMsg.Send(memberEntity->GetSystemAddress());
	}
}

// Drops a mission item for a player
// If the player does not need this item, it will not be dropped.
void DropMissionLoot(Entity& player, GameMessages::DropClientLoot& lootMsg) {
	GameMessages::MissionNeedsLot needMsg{};
	needMsg.item = lootMsg.item;
	const auto playerID = player.GetObjectID();
	needMsg.target = playerID;
	// Will return false if the item is not required
	if (needMsg.Send()) {
		lootMsg.target = playerID;
		lootMsg.ownerID = playerID;
		lootMsg.lootID = ObjectIDManager::GenerateObjectID();
		CalcFinalDropPos(lootMsg);
		// Register the drop with the player
		lootMsg.Send();
		// Visually drop the loot to be picked up
		lootMsg.Send(player.GetSystemAddress());
	}
}

// Check if the item needs to be dropped for anyone on the team
// Only players who need the item will have it dropped
void DropMissionLoot(const Team& team, GameMessages::DropClientLoot& lootMsg) {
	GameMessages::MissionNeedsLot needMsg{};
	needMsg.item = lootMsg.item;
	for (const auto member : team.members) {
		GameMessages::GetPosition memberPosMsg{};
		memberPosMsg.target = member;
		memberPosMsg.Send();
		if (NiPoint3::Distance(memberPosMsg.pos, lootMsg.spawnPos) > g_MAX_DROP_RADIUS) continue;

		needMsg.target = member;
		// Will return false if the item is not required
		if (needMsg.Send()) {
			lootMsg.target = member;
			lootMsg.ownerID = member;
			lootMsg.lootID = ObjectIDManager::GenerateObjectID();
			CalcFinalDropPos(lootMsg);
			// Register the drop with the player
			lootMsg.Send();
			DistrbuteMsgToTeam(lootMsg, team);
		}
	}
}

// Drop a regular piece of loot.
// Most items will go through this.
// A player will always get a drop that goes through this function
void DropRegularLoot(Entity& player, GameMessages::DropClientLoot& lootMsg) {
	const auto playerID = player.GetObjectID();

	CalcFinalDropPos(lootMsg);

	lootMsg.lootID = ObjectIDManager::GenerateObjectID();
	lootMsg.target = playerID;
	lootMsg.ownerID = playerID;
	// Register the drop with the player
	lootMsg.Send();
	// Visually drop the loot to be picked up
	lootMsg.Send(player.GetSystemAddress());

}

// Drop a regular piece of loot.
// Most items will go through this.
// Finds the next loot owner on the team the is in range of the kill and gives them this reward.
void DropRegularLoot(Team& team, GameMessages::DropClientLoot& lootMsg) {
	auto earningPlayer = LWOOBJID_EMPTY;
	lootMsg.lootID = ObjectIDManager::GenerateObjectID();
	CalcFinalDropPos(lootMsg);
	GameMessages::GetPosition memberPosMsg{};
	// Find the next loot owner.  Eventually this will run into the `player` passed into this function, since those will
	// have the same ID, this loop will only ever run at most 4 times.
	do {
		earningPlayer = team.GetNextLootOwner();
		memberPosMsg.target = earningPlayer;
		memberPosMsg.Send();
	} while (NiPoint3::Distance(memberPosMsg.pos, lootMsg.spawnPos) > g_MAX_DROP_RADIUS);

	if (team.lootOption == 0 /* Shared loot */) {
		lootMsg.target = earningPlayer;
		lootMsg.ownerID = earningPlayer;
		lootMsg.Send();
	} else /* Free for all loot */ {
		lootMsg.ownerID = LWOOBJID_EMPTY;
		// By sending the loot with NO owner and to ALL members of the team,
		// its a first come, first serve with who picks the item up.
		for (const auto ffaMember : team.members) {
			lootMsg.target = ffaMember;
			lootMsg.Send();
		}
	}

	DistrbuteMsgToTeam(lootMsg, team);
}

void DropLoot(Entity* player, const LWOOBJID source, const std::map<LOT, LootDropInfo>& rolledItems, uint32_t minCoins, uint32_t maxCoins) {
	player = player->GetOwner(); // if the owner is overwritten, we collect that here
	const auto playerID = player->GetObjectID();
	if (!player || !player->IsPlayer()) {
		LOG("Trying to drop loot for non-player %llu:%i", playerID, player->GetLOT());
		return;
	}

	// TODO should be scene based instead of radius based
	// drop loot to either single player or team
	// powerups never have an owner when dropped
	// for every player on the team in a radius of 700 (arbitrary value, not lore)
	// if shared loot, drop everything but tokens to the next team member that gets loot,
	// then tokens to everyone (1 token drop in a 3 person team means everyone gets a token)
	// if Free for all, drop everything with NO owner, except tokens which follow the same logic as above
	auto* team = TeamManager::Instance()->GetTeam(playerID);

	GameMessages::GetPosition posMsg;
	posMsg.target = source;
	posMsg.Send();

	const auto spawnPosition = posMsg.pos;
	auto* const objectsTable = CDClientManager::GetTable<CDObjectsTable>();

	constexpr LOT TOKEN_PROXY = 13763;
	// Go through the drops 1 at a time to drop them
	for (auto it = rolledItems.begin(); it != rolledItems.end(); it++) {
		auto& [lootLot, info] = *it;
		for (int i = 0; i < info.count; i++) {
			GameMessages::DropClientLoot lootMsg{};
			lootMsg.spawnPos = spawnPosition;
			lootMsg.sourceID = source;
			lootMsg.item = lootLot;
			lootMsg.count = 1;
			lootMsg.currency = 0;
			const CDObjects& object = objectsTable->GetByID(lootLot);

			if (lootLot == TOKEN_PROXY) {
				team ? DropFactionLoot(*team, lootMsg) : DropFactionLoot(*player, lootMsg);
			} else if (info.table.MissionDrop) {
				team ? DropMissionLoot(*team, lootMsg) : DropMissionLoot(*player, lootMsg);
			} else if (object.type == "Powerup") {
				team ? DropPowerupLoot(*team, lootMsg) : DropPowerupLoot(*player, lootMsg);
			} else {
				team ? DropRegularLoot(*team, lootMsg) : DropRegularLoot(*player, lootMsg);
			}
		}
	}

	// Coin roll is divided up between the members, rounded up, then dropped for each player
	const uint32_t coinRoll = static_cast<uint32_t>(minCoins + GeneralUtils::GenerateRandomNumber<float>(0, 1) * (maxCoins - minCoins));
	const auto droppedCoins = team ? std::ceil(coinRoll / team->members.size()) : coinRoll;
	if (team) {
		for (auto member : team->members) {
			GameMessages::DropClientLoot lootMsg{};
			lootMsg.target = member;
			lootMsg.ownerID = member;
			lootMsg.currency = droppedCoins;
			lootMsg.spawnPos = spawnPosition;
			lootMsg.sourceID = source;
			lootMsg.item = LOT_NULL;
			CalcFinalDropPos(lootMsg);
			lootMsg.Send();
			const auto* const memberEntity = Game::entityManager->GetEntity(member);
			if (memberEntity) lootMsg.Send(memberEntity->GetSystemAddress());
		}
	} else {
		GameMessages::DropClientLoot lootMsg{};
		lootMsg.target = playerID;
		lootMsg.ownerID = playerID;
		lootMsg.currency = droppedCoins;
		lootMsg.spawnPos = spawnPosition;
		lootMsg.sourceID = source;
		lootMsg.item = LOT_NULL;
		CalcFinalDropPos(lootMsg);
		lootMsg.Send();
		lootMsg.Send(player->GetSystemAddress());
	}
}

void Loot::DropItem(Entity& player, GameMessages::DropClientLoot& lootMsg, bool useTeam, bool forceFfa) {
	auto* const team = useTeam ? TeamManager::Instance()->GetTeam(player.GetObjectID()) : nullptr;
	char oldTeamLoot{};
	if (team && forceFfa) {
		oldTeamLoot = team->lootOption;
		team->lootOption = 1;
	}

	auto* const objectsTable = CDClientManager::GetTable<CDObjectsTable>();
	const CDObjects& object = objectsTable->GetByID(lootMsg.item);

	constexpr LOT TOKEN_PROXY = 13763;
	if (lootMsg.item == TOKEN_PROXY) {
		team ? DropFactionLoot(*team, lootMsg) : DropFactionLoot(player, lootMsg);
	} else if (object.type == "Powerup") {
		team ? DropPowerupLoot(*team, lootMsg) : DropPowerupLoot(player, lootMsg);
	} else {
		team ? DropRegularLoot(*team, lootMsg) : DropRegularLoot(player, lootMsg);
	}

	if (team) team->lootOption = oldTeamLoot;
}

void Loot::CacheMatrix(uint32_t matrixIndex) {
	if (CachedMatrices.contains(matrixIndex)) return;

	CachedMatrices.insert(matrixIndex);
	CDComponentsRegistryTable* componentsRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	CDItemComponentTable* itemComponentTable = CDClientManager::GetTable<CDItemComponentTable>();
	CDLootMatrixTable* lootMatrixTable = CDClientManager::GetTable<CDLootMatrixTable>();
	CDLootTableTable* lootTableTable = CDClientManager::GetTable<CDLootTableTable>();
	CDRarityTableTable* rarityTableTable = CDClientManager::GetTable<CDRarityTableTable>();

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

Loot::Return Loot::RollLootMatrix(Entity* player, uint32_t matrixIndex) {
	auto* const missionComponent = player ? player->GetComponent<MissionComponent>() : nullptr;

	Loot::Return toReturn;
	const auto drops = ::RollLootMatrix(matrixIndex);
	// if no mission component, just convert the map and skip checking if its a mission drop
	if (!missionComponent) {
		for (const auto& [lot, info] : drops) toReturn[lot] = info.count;
	} else {
		for (const auto& [lot, info] : drops) {
			const auto& itemInfo = info.table;

			// filter out uneeded mission items
			if (itemInfo.MissionDrop && !missionComponent->RequiresItem(itemInfo.itemid))
				continue;

			LOT itemLot = lot;
			// convert faction token proxy
			if (itemLot == 13763) {
				if (missionComponent->GetMissionState(545) == eMissionState::COMPLETE)
					itemLot = 8318; // "Assembly Token"
				else if (missionComponent->GetMissionState(556) == eMissionState::COMPLETE)
					itemLot = 8321; // "Venture League Token"
				else if (missionComponent->GetMissionState(567) == eMissionState::COMPLETE)
					itemLot = 8319; // "Sentinels Token"
				else if (missionComponent->GetMissionState(578) == eMissionState::COMPLETE)
					itemLot = 8320; // "Paradox Token"
			}

			if (itemLot == 13763) {
				continue;
			} // check if we aren't in faction

			toReturn[itemLot] = info.count;
		}
	}

	if (player) {
		for (const auto& [lot, count] : toReturn) {
			LOG("Player %llu has rolled %i of item %i from loot matrix %i", player->GetObjectID(), count, lot, matrixIndex);
		}
	}

	return toReturn;
}

void Loot::GiveLoot(Entity* player, uint32_t matrixIndex, eLootSourceType lootSourceType) {
	player = player->GetOwner(); // If the owner is overwritten, we collect that here

	const auto result = RollLootMatrix(player, matrixIndex);

	GiveLoot(player, result, lootSourceType);
}

void Loot::GiveLoot(Entity* player, const Loot::Return& result, eLootSourceType lootSourceType) {
	player = player->GetOwner(); // if the owner is overwritten, we collect that here

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	if (!inventoryComponent)
		return;

	for (const auto& pair : result) {
		inventoryComponent->AddItem(pair.first, pair.second, lootSourceType);
	}
}

void Loot::GiveActivityLoot(Entity* player, const LWOOBJID source, uint32_t activityID, int32_t rating) {
	CDActivityRewardsTable* activityRewardsTable = CDClientManager::GetTable<CDActivityRewardsTable>();
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

	CDCurrencyTableTable* currencyTableTable = CDClientManager::GetTable<CDCurrencyTableTable>();
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

void Loot::DropLoot(Entity* player, const LWOOBJID source, uint32_t matrixIndex, uint32_t minCoins, uint32_t maxCoins) {
	player = player->GetOwner(); // if the owner is overwritten, we collect that here

	auto* inventoryComponent = player->GetComponent<InventoryComponent>();

	if (!inventoryComponent)
		return;

	const auto result = ::RollLootMatrix(matrixIndex);

	::DropLoot(player, source, result, minCoins, maxCoins);
}

void Loot::DropActivityLoot(Entity* player, const LWOOBJID source, uint32_t activityID, int32_t rating) {
	CDActivityRewardsTable* activityRewardsTable = CDClientManager::GetTable<CDActivityRewardsTable>();
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

	CDCurrencyTableTable* currencyTableTable = CDClientManager::GetTable<CDCurrencyTableTable>();
	std::vector<CDCurrencyTable> currencyTable = currencyTableTable->Query([selectedReward](CDCurrencyTable entry) { return (entry.currencyIndex == selectedReward->CurrencyIndex && entry.npcminlevel == 1); });

	if (currencyTable.size() > 0) {
		minCoins = currencyTable[0].minvalue;
		maxCoins = currencyTable[0].maxvalue;
	}

	DropLoot(player, source, selectedReward->LootMatrixIndex, minCoins, maxCoins);
}
