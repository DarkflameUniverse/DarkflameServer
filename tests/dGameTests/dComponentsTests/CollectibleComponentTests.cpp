// CollectibleComponent.requirement_mission (a server-only table): collectibles of a mission to accept only count while
// the player has that mission.
#include "GameDependencies.h"

#include "GameMessages.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDCollectibleComponentTable.h"
#include "CDMissionsTable.h"
#include "CollectibleComponent.h"
#include "Entity.h"
#include "MissionComponent.h"

#include "tinyxml2.h"

#include <gtest/gtest.h>

class CollectibleComponentTests : public GameDependenciesTest {
protected:
	// Rows as in the 1.10.64 CDClient: 78 is the Ice Dragon Relic (requirement 2040, a mission to accept), 12 the Pirate
	// Flag (requirement 12, an achievement), 45 a test item (66666666, no such mission), 1 none
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:"); // MissionComponent counts the achievements
		CDClientDatabase::ExecuteDML("CREATE TABLE Missions (id INTEGER, isMission INTEGER);");
		auto& collectibles = CDClientManager::GetEntriesMutable<CDCollectibleComponentTable>();
		collectibles[78] = { 78, 2040 };
		collectibles[12] = { 12, 12 };
		collectibles[45] = { 45, 66666666 };
		collectibles[1] = { 1, -1 };
		auto& missions = CDClientManager::GetEntriesMutable<CDMissionsTable>();
		CDMissions relics{};
		relics.id = 2040;
		relics.isMission = true;
		missions.push_back(relics);
		CDMissions flags{};
		flags.id = 12;
		flags.isMission = false;
		missions.push_back(flags);
	}

	void TearDown() override {
		CDClientManager::GetEntriesMutable<CDCollectibleComponentTable>().clear();
		CDClientManager::GetEntriesMutable<CDMissionsTable>().clear();
		TearDownDependencies();
	}

	static void GiveMission(MissionComponent& missions, const char* state) {
		tinyxml2::XMLDocument doc;
		const std::string xml = std::string(R"(<obj v="1"><mis><done/><cur><m id="2040" state=")") + state + R"("/></cur></mis></obj>)";
		ASSERT_EQ(doc.Parse(xml.c_str()), tinyxml2::XML_SUCCESS);
		missions.LoadFromXml(doc);
	}
};

TEST_F(CollectibleComponentTests, RequirementMissionIsRead) {
	Entity object(40, info);
	EXPECT_EQ(object.AddComponent<CollectibleComponent>(78, 5)->GetRequirementMission(), 2040);
	Entity other(41, info);
	EXPECT_EQ(other.AddComponent<CollectibleComponent>(999, 5)->GetRequirementMission(), -1); // no row
}

TEST_F(CollectibleComponentTests, AchievementsAndMissingMissionsAlwaysCount) {
	Entity player(42, info);
	player.AddComponent<MissionComponent>(-1);
	for (const int32_t component : { 12, 45, 1, 999 }) {
		Entity object(43, info);
		EXPECT_TRUE(object.AddComponent<CollectibleComponent>(component, 5)->CountsFor(player)) << component;
	}
}

TEST_F(CollectibleComponentTests, MissionCollectiblesCountOnlyWhileTheMissionIsAccepted) {
	Entity relic(44, info);
	const auto* const collectible = relic.AddComponent<CollectibleComponent>(78, 5);

	Entity player(45, info);
	auto* const missions = player.AddComponent<MissionComponent>(-1);
	EXPECT_FALSE(collectible->CountsFor(player)); // never offered

	GiveMission(*missions, "2"); // ACTIVE
	EXPECT_TRUE(collectible->CountsFor(player));
	GiveMission(*missions, "10"); // COMPLETE_ACTIVE (a repeat)
	EXPECT_TRUE(collectible->CountsFor(player));
	GiveMission(*missions, "1"); // AVAILABLE: offered, not accepted
	EXPECT_FALSE(collectible->CountsFor(player));

	Entity noMissions(46, info);
	EXPECT_FALSE(collectible->CountsFor(noMissions));
}
