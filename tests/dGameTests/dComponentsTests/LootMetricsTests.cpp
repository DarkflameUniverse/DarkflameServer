#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "CDClientDatabase.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "Entity.h"
#include "EntityManager.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"
#include "Item.h"
#include "LootMetrics.h"
#include "QuickBuildComponent.h"
#include "MissionComponent.h"
#include "DestroyableComponent.h"
#include "LevelProgressionComponent.h"
#include "eQuickBuildState.h"
#include "eReplicaComponentType.h"

#include "dGameMessagesTests/GameMessageTestUtils.h"

using namespace GameMessageTestUtils;

// The _Metric_ keys live put in AddItemToInventoryClientSync's extra info saying where items came from
// (LootMetrics), and the loot source of RemoveItemFromInventory.
class LootMetricsTests : public GameDependenciesTest {
protected:
	static constexpr LOT ITEM = 7000;
	static constexpr LOT SMASHABLE = 4712;
	static constexpr LWOOBJID PLAYER = 0x1000000000000001LL;

	std::unique_ptr<Entity> entity;
	std::unique_ptr<Character> character;
	InventoryComponent* inventory{};

	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:");
		CDClientDatabase::ExecuteDML("CREATE TABLE ItemSets (setID INTEGER, itemIDs TEXT);");
		CDClientDatabase::ExecuteDML("CREATE TABLE Objects (id INTEGER, name TEXT, type TEXT);");
		CDClientDatabase::ExecuteDML("CREATE TABLE ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER);");
		CDClientDatabase::ExecuteDML("CREATE TABLE ObjectSkills (objectTemplate INTEGER, skillID INTEGER, castOnType INTEGER, AICombatWeight INTEGER);");
		CDClientDatabase::ExecuteDML("CREATE TABLE SkillBehavior (skillID INTEGER, behaviorID INTEGER);");
		CDClientDatabase::ExecuteDML("CREATE TABLE Missions (id INTEGER, isMission INTEGER);");
		RegisterLot(ITEM);

		info.lot = 1; // a player
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(PLAYER, info);
		inventory = entity->AddComponent<InventoryComponent>(-1);
		character = std::make_unique<Character>(1, nullptr);
		entity->SetCharacter(character.get());
		entity->AddComponent<CharacterComponent>(-1, character.get(), ClientAddress())->InitializeStatisticsFromString("");
		Game::entityManager->_addEntity(entity.get()); // drops from the player, and the quickbuild, look it up there
		ClearSent();
	}

	void TearDown() override {
		Game::entityManager->_removeEntity(PLAYER);
		entity->SetCharacter(nullptr);
		entity.reset();
		character.reset();
		TearDownDependencies();
	}

	static void RegisterLot(const LOT lot) {
		const auto componentID = static_cast<uint32_t>(96000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.stackSize = 999;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	static dServerMock& Server() { return *static_cast<dServerMock*>(Game::server); }

	static void ClearSent() { Server().ClearSentPackets(); }

	// The extra info of every AddItemToInventoryClientSync sent
	static std::vector<std::u16string> AddedExtraInfo() {
		std::vector<std::u16string> out;
		for (const auto& packet : Server().GetSentPackets()) {
			RakNet::BitStream bitStream(const_cast<uint8_t*>(packet.bytes.data()), packet.bytes.size(), false);
			LWOOBJID target{};
			MessageType::Game id{};
			if (!GameMessages::NetGameMsg::ReadPacketHeader(bitStream, target, id)) continue;
			if (id != MessageType::Game::ADD_ITEM_TO_INVENTORY_CLIENT_SYNC) continue;
			GameMessages::AddItemToInventoryClientSync msg;
			EXPECT_TRUE(msg.Deserialize(bitStream));
			out.push_back(msg.extraInfo);
		}
		return out;
	}

	static std::vector<GameMessages::RemoveItemFromInventory> Removed() {
		std::vector<GameMessages::RemoveItemFromInventory> out;
		for (const auto& packet : Server().GetSentPackets()) {
			RakNet::BitStream bitStream(const_cast<uint8_t*>(packet.bytes.data()), packet.bytes.size(), false);
			LWOOBJID target{};
			MessageType::Game id{};
			if (!GameMessages::NetGameMsg::ReadPacketHeader(bitStream, target, id)) continue;
			if (id != MessageType::Game::REMOVE_ITEM_FROM_INVENTORY) continue;
			auto& msg = out.emplace_back();
			EXPECT_TRUE(msg.Deserialize(bitStream));
		}
		return out;
	}

	// Drops an item for the player from the given source object and picks it up
	void DropAndPickUp(const LWOOBJID source, const LWOOBJID lootId) {
		GameMessages::DropClientLoot drop;
		drop.target = PLAYER;
		drop.ownerID = PLAYER;
		drop.sourceID = source;
		drop.item = ITEM;
		drop.count = 1;
		drop.lootID = lootId;
		GameMessages::DropClientLootEvent event(drop);
		entity->MsgDropClientLoot(event);
		ClearSent();
		entity->PickupItem(lootId);
	}
};

TEST_F(LootMetricsTests, KeysAsLiveWroteThem) {
	EXPECT_TRUE(LootMetrics{}.Empty());
	EXPECT_EQ(LootMetrics{}.ToExtraInfo(), u"");

	LootMetrics pickup{};
	pickup.sourceLot = 1;
	EXPECT_EQ(pickup.ToExtraInfo(), u"_Metric_Souce_LOT_Int=1:1");

	LootMetrics mission{};
	mission.sourceLot = 1;
	mission.missionId = 889;
	EXPECT_EQ(mission.ToExtraInfo(), u"_Metric_Souce_LOT_Int=1:1,_Metric_Mission_ID_Int=1:889");

	LootMetrics vendor{};
	vendor.sourceLot = 13380;
	vendor.currencyDelta = -20000;
	EXPECT_EQ(vendor.ToExtraInfo(), u"_Metric_Souce_LOT_Int=1:13380,_Metric_Currency_Delta_Int=1:-20000");

	// Live: the mail ID as name value type 8, the trade ID as type 9
	LootMetrics mail{};
	mail.mailId = 1152921510716557582ULL;
	EXPECT_EQ(mail.ToExtraInfo(), u"_Metric_Mail_ID_Int64=8:1152921510716557582");
	LootMetrics trade{};
	trade.transactionId = 1152921510834827094LL;
	EXPECT_EQ(trade.ToExtraInfo(), u"_Metric_Transaction_ID_Int64=9:1152921510834827094");
}

TEST_F(LootMetricsTests, PickupSaysWhichObjectItDroppedFrom) {
	info.lot = SMASHABLE;
	auto* const smashable = Game::entityManager->CreateEntity(info, nullptr, nullptr, false, 0x40040000000100LL);
	ASSERT_NE(smashable, nullptr);

	DropAndPickUp(smashable->GetObjectID(), 0x5000);
	EXPECT_EQ(AddedExtraInfo(), std::vector<std::u16string>{ u"_Metric_Souce_LOT_Int=1:4712" });

	// A second one joins the stack; the message still says where it came from
	DropAndPickUp(smashable->GetObjectID(), 0x5001);
	EXPECT_EQ(AddedExtraInfo(), std::vector<std::u16string>{ u"_Metric_Souce_LOT_Int=1:4712" });
}

TEST_F(LootMetricsTests, PlayerSourcedPickupIsLotOne) {
	// Activity rewards and chests drop from the player (live: 1 on 741 pickups)
	DropAndPickUp(PLAYER, 0x5002);
	EXPECT_EQ(AddedExtraInfo(), std::vector<std::u16string>{ u"_Metric_Souce_LOT_Int=1:1" });
}

TEST_F(LootMetricsTests, UnknownSourceSendsNoKey) {
	DropAndPickUp(0x40040000000999LL, 0x5003);
	EXPECT_EQ(AddedExtraInfo(), std::vector<std::u16string>{ u"" });
}

TEST_F(LootMetricsTests, MetricsFollowTheItemConfig) {
	LwoNameValue config;
	config.Insert<int32_t>(u"a", 5);
	LootMetrics metrics{};
	metrics.sourceLot = 1;
	inventory->AddItem(ITEM, 1, eLootSourceType::PICKUP, eInventoryType::INVALID, config, LWOOBJID_EMPTY, true, false, LWOOBJID_EMPTY, eInventoryType::INVALID, 0, false, -1, metrics);
	EXPECT_EQ(AddedExtraInfo(), std::vector<std::u16string>{ u"a=1:5,_Metric_Souce_LOT_Int=1:1" });
	// Only the message carries them
	const auto* const item = inventory->FindItemByLot(ITEM);
	ASSERT_NE(item, nullptr);
	EXPECT_EQ(item->GetConfig().values.size(), 1u);
}

TEST_F(LootMetricsTests, RemovalSaysWhatTookTheItems) {
	inventory->AddItem(ITEM, 5, eLootSourceType::NONE);
	ClearSent();
	constexpr LWOOBJID QUICKBUILD = 0x40040000000200LL;
	ASSERT_TRUE(inventory->RemoveItem(ITEM, 5, eInventoryType::ALL, false, false, { eLootSourceType::QUICKBUILD, QUICKBUILD }));
	const auto removed = Removed();
	ASSERT_EQ(removed.size(), 1u);
	EXPECT_EQ(removed[0].eLootTypeSource, static_cast<int32_t>(eLootSourceType::QUICKBUILD));
	EXPECT_EQ(removed[0].iLootTypeSource, QUICKBUILD);
	EXPECT_EQ(removed[0].iStackCount, 5u);

	// Without a source, as before
	inventory->AddItem(ITEM, 1, eLootSourceType::NONE);
	ClearSent();
	ASSERT_TRUE(inventory->RemoveItem(ITEM, 1));
	ASSERT_EQ(Removed().size(), 1u);
	EXPECT_EQ(Removed()[0].eLootTypeSource, LOOTTYPE_NONE);
	EXPECT_EQ(Removed()[0].iLootTypeSource, LWOOBJID_EMPTY);
}

// Live: the FV Stone Warrior pedestal's 5 Maelstrom Infected Bricks were taken with the loot source Quickbuild and the
// quickbuild as the source object
TEST_F(LootMetricsTests, QuickbuildTakesItsCostAsTheSource) {
	constexpr LOT BRICK = 6194;
	constexpr int32_t PRECONDITION = 90099; // stands in for 99: have 5 of LOT 6194
	RegisterLot(BRICK);
	CDClientDatabase::ExecuteDML("CREATE TABLE IF NOT EXISTS Preconditions (id INTEGER, type INTEGER, targetLOT TEXT, targetCount INTEGER);");
	CDClientDatabase::ExecuteDML("INSERT INTO Preconditions VALUES (" + std::to_string(PRECONDITION) + ", 2, '6194', 5);");
	inventory->AddItem(BRICK, 7, eLootSourceType::NONE);
	// What the precondition check looks at besides the inventory
	entity->AddComponent<MissionComponent>(-1);
	entity->AddComponent<DestroyableComponent>(-1);
	entity->AddComponent<LevelProgressionComponent>(-1);

	auto pedestalInfo = info;
	pedestalInfo.lot = 8551;
	auto pedestal = std::make_unique<Entity>(0x0102030405060708LL, pedestalInfo);
	pedestal->SetVar<std::u16string>(u"CheckPrecondition", GeneralUtils::ASCIIToUTF16(std::to_string(PRECONDITION)));
	auto* const quickBuild = pedestal->AddComponent<QuickBuildComponent>(-1);
	quickBuild->SetResetTime(20.0f);
	quickBuild->SetCompleteTime(10.0f);
	ClearSent();

	quickBuild->OnUse(entity.get());
	EXPECT_EQ(quickBuild->GetState(), eQuickBuildState::BUILDING);
	const auto removed = Removed();
	ASSERT_EQ(removed.size(), 1u);
	EXPECT_EQ(removed[0].iObjTemplate, BRICK);
	EXPECT_EQ(removed[0].iStackCount, 5u);
	EXPECT_EQ(removed[0].eLootTypeSource, static_cast<int32_t>(eLootSourceType::QUICKBUILD));
	EXPECT_EQ(removed[0].iLootTypeSource, pedestal->GetObjectID());
	EXPECT_EQ(inventory->GetLotCount(BRICK), 2u);

	pedestal.reset();
}
