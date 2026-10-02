// Whether a mission's rewards fit before it is turned in, as the 1.10.64 client checks
// (LWOMissionComponent::msgCanReceiveAllRewards 0x00d17e20, LWOInventoryComponent_Common::CheckCanAddItems 0x00d53de0).
#include "GameDependencies.h"

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "CDMissionTasksTable.h"
#include "CDMissionsTable.h"
#include "Entity.h"
#include "GameMessages.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "Mission.h"
#include "MissionComponent.h"
#include "eItemType.h"
#include "eMissionTaskType.h"
#include "eReplicaComponentType.h"

#include "tinyxml2.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>

class MissionRewardSpaceTests : public GameDependenciesTest {
protected:
	static constexpr LOT HAT = 8100;    // stack 1
	static constexpr LOT GEM = 8101;    // stack 10
	static constexpr LOT UNIQUE = 8102; // unique
	static constexpr LOT DEED = 8103;   // stack 0: no limit
	static constexpr uint32_t PLAIN = 993001;  // rewards a hat; the repeat rewards 2 hats
	static constexpr uint32_t GATHER = 993002; // takes 2 gems, rewards a hat
	static constexpr uint32_t KEEP = 993003;   // gathers 2 gems but keeps them, rewards a hat
	static constexpr uint32_t BAG = 993004;    // rewards a hat and one more slot
	static constexpr uint32_t GEMS = 993005;   // rewards 5 gems
	static constexpr uint32_t ONE = 993006;    // rewards the unique item
	static constexpr uint32_t CHOICE = 993007; // rewards a hat or 5 gems

	std::unique_ptr<Entity> player;
	InventoryComponent* inventory{};
	MissionComponent* missions{};
	LWOOBJID nextId = 0x9000;

	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:"); // MissionComponent counts the achievements
		CDClientDatabase::ExecuteDML("CREATE TABLE Missions (id INTEGER, isMission INTEGER);");
		RegisterLot(HAT, 1, false);
		RegisterLot(GEM, 10, false);
		RegisterLot(UNIQUE, 1, true);
		RegisterLot(DEED, 0, false);
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);

		auto& rows = CDClientManager::GetEntriesMutable<CDMissionsTable>();
		rows.push_back(Rewards(PLAIN, HAT, 1));
		rows.back().repeatable = true;
		rows.back().reward_item1_repeatable = HAT;
		rows.back().reward_item1_repeat_count = 2;
		rows.push_back(Rewards(GATHER, HAT, 1));
		rows.push_back(Rewards(KEEP, HAT, 1));
		rows.push_back(Rewards(BAG, HAT, 1));
		rows.back().reward_maxinventory = 1;
		rows.push_back(Rewards(GEMS, GEM, 5));
		rows.push_back(Rewards(ONE, UNIQUE, 1));
		rows.push_back(Rewards(CHOICE, HAT, 1));
		rows.back().reward_item2 = GEM;
		rows.back().reward_item2_count = 5;

		auto& tasks = CDClientManager::GetEntriesMutable<CDMissionTasksTable>();
		tasks.push_back(GatherTask(GATHER, ""));
		tasks.push_back(GatherTask(KEEP, "1"));

		player = std::make_unique<Entity>(48, info);
		inventory = player->AddComponent<InventoryComponent>(-1);
		missions = player->AddComponent<MissionComponent>(-1);
	}

	void TearDown() override {
		player.reset();
		CDClientManager::GetEntriesMutable<CDMissionsTable>().clear();
		CDClientManager::GetEntriesMutable<CDMissionTasksTable>().clear();
		TearDownDependencies();
	}

	static CDMissions Rewards(const uint32_t id, const LOT lot, const int32_t count) {
		CDMissions row{};
		row.id = id;
		row.isMission = true;
		row.cooldownTime = -1;
		row.offer_objectID = 1000;
		row.target_objectID = 1000;
		for (auto* const item : { &row.reward_item1, &row.reward_item2, &row.reward_item3, &row.reward_item4,
			&row.reward_item1_repeatable, &row.reward_item2_repeatable, &row.reward_item3_repeatable, &row.reward_item4_repeatable }) *item = LOT_NULL;
		row.reward_item1 = lot;
		row.reward_item1_count = count;
		return row;
	}

	static CDMissionTasks GatherTask(const uint32_t missionId, const std::string& param) {
		CDMissionTasks task{};
		task.id = missionId;
		task.taskType = static_cast<uint32_t>(eMissionTaskType::GATHER);
		task.target = GEM;
		task.targetValue = 2;
		task.taskParam1 = param;
		return task;
	}

	static void RegisterLot(const LOT lot, const uint32_t stack, const bool unique) {
		const auto componentID = static_cast<uint32_t>(97000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.itemType = static_cast<uint32_t>(eItemType::HAT);
		component.stackSize = stack;
		component.isUnique = unique;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	void Give(const LOT lot, const uint32_t count, const eInventoryType type = eInventoryType::ITEMS) {
		auto* const bag = inventory->GetInventory(type);
		new Item(nextId++, lot, bag, bag->FindEmptySlot(), count, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	}

	// Fills the items bag with hats up to all but `free` slots
	void Fill(const int32_t free) {
		auto* const bag = inventory->GetInventory(eInventoryType::ITEMS);
		while (bag->GetEmptySlots() > free) Give(HAT, 1);
	}

	// The mission ready to turn in (the first time, or a repeat)
	const Mission& Ready(const uint32_t id, const bool repeat = false) {
		tinyxml2::XMLDocument doc;
		const auto ids = std::to_string(id);
		const auto done = repeat ? "<m id=\"" + ids + "\" state=\"8\" cct=\"1\" cts=\"1000\"/>" : std::string();
		const auto xml = "<obj v=\"1\"><mis><done>" + done + "</done><cur><m id=\"" + ids + "\" state=\"" + (repeat ? "12" : "4") + "\"/></cur></mis></obj>";
		doc.Parse(xml.c_str());
		missions->LoadFromSaveData(doc);
		return *missions->GetMission(id);
	}
};

TEST_F(MissionRewardSpaceTests, RewardNeedsAFreeSlot) {
	const auto& mission = Ready(PLAIN);
	Fill(1);
	EXPECT_TRUE(mission.CanReceiveAllRewards());
	Fill(0);
	EXPECT_FALSE(mission.CanReceiveAllRewards());
}

TEST_F(MissionRewardSpaceTests, RepeatRewardsAndTheBagReward) {
	Fill(1);
	EXPECT_FALSE(Ready(PLAIN, true).CanReceiveAllRewards()); // 2 hats the second time
	Fill(0);
	EXPECT_TRUE(Ready(BAG).CanReceiveAllRewards()); // the reward's own extra slot
}

TEST_F(MissionRewardSpaceTests, TakenItemsMakeRoom) {
	Fill(1);
	Give(GEM, 1); // the last slot; taking one gem settles the whole count, as the client
	EXPECT_TRUE(Ready(GATHER).CanReceiveAllRewards());
	EXPECT_FALSE(Ready(KEEP).CanReceiveAllRewards());
}

TEST_F(MissionRewardSpaceTests, StacksAreToppedUp) {
	Fill(1);
	Give(GEM, 5);
	EXPECT_TRUE(Ready(GEMS).CanReceiveAllRewards());
	EXPECT_TRUE(inventory->CanAddItems({ { GEM, 5 } }, {}, {}));
	EXPECT_FALSE(inventory->CanAddItems({ { GEM, 6 } }, {}, {}));
	EXPECT_FALSE(inventory->CanAddItems({ { GEM, 5 }, { GEM, 1 } }, {}, {}));
	EXPECT_FALSE(inventory->CanAddItems({ { DEED, 1000 } }, { { GEM, 4 } }, {})); // the gems keep their slot
	EXPECT_TRUE(inventory->CanAddItems({ { DEED, 1000 } }, { { GEM, 5 } }, {})); // a stack size of 0 is no limit
	EXPECT_TRUE(inventory->CanAddItems({ { HAT, 1 } }, {}, { { eInventoryType::ITEMS, 1 } }));
}

TEST_F(MissionRewardSpaceTests, OneUniqueItem) {
	const auto& mission = Ready(ONE);
	EXPECT_TRUE(mission.CanReceiveAllRewards());
	Give(UNIQUE, 1, eInventoryType::VENDOR_BUYBACK);
	EXPECT_TRUE(mission.CanReceiveAllRewards()); // only sold back
	Give(UNIQUE, 1, eInventoryType::VAULT_ITEMS);
	EXPECT_FALSE(mission.CanReceiveAllRewards());
}

TEST_F(MissionRewardSpaceTests, OnlyTheChosenReward) {
	const auto& mission = Ready(CHOICE);
	Fill(1);
	EXPECT_FALSE(mission.CanReceiveAllRewards()); // a hat and the gems need two slots
	EXPECT_TRUE(mission.CanReceiveAllRewards(HAT));
	EXPECT_TRUE(mission.CanReceiveAllRewards(GEM));
}

TEST_F(MissionRewardSpaceTests, NotInProgress) {
	tinyxml2::XMLDocument doc;
	doc.Parse(R"(<obj v="1"><mis><done><m id="993001" state="8" cct="1" cts="1000"/></done><cur/></mis></obj>)");
	missions->LoadFromSaveData(doc);
	EXPECT_FALSE(missions->GetMission(PLAIN)->CanReceiveAllRewards());
}
