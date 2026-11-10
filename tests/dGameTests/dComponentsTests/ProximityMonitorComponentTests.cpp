#include "GameDependencies.h"

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDProximityMonitorComponentTable.h"
#include "Entity.h"
#include "GameMessages.h"
#include "ProximityMonitorComponent.h"
#include "dpEntity.h"

#include <gtest/gtest.h>

// A ProximityMonitorComponent row lists ProximityTypes ids, as LWOProximityMonitorComponent::LoadDataFromTemplate
// (0x00c2dfb0) reads it; the types say their name, radius and where they are loaded.
class ProximityMonitorComponentTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:");
		CDClientDatabase::ExecuteDML("CREATE TABLE ProximityTypes (id INTEGER, Name TEXT, Radius INTEGER, CollisionGroup INTEGER, PassiveChecks INTEGER, IconID INTEGER, LoadOnClient INTEGER, LoadOnServer INTEGER);");
		CDClientDatabase::ExecuteDML("INSERT INTO ProximityTypes VALUES (2, 'Test Proximity', 10, 1, 1, 1, 1, 0);");
		CDClientDatabase::ExecuteDML("INSERT INTO ProximityTypes VALUES (3, 'Test Prox Large', 20, 1, 1, 3, 1, 1);");
		CDClientDatabase::ExecuteDML("INSERT INTO ProximityTypes VALUES (7, 'RocketSign', 30, 1, 1, -1, 1, 0);");
		CDClientDatabase::ExecuteDML("INSERT INTO ProximityTypes VALUES (8, 'WorldLauncher', 80, 1, 1, 74, 1, 0);");
		CDClientDatabase::ExecuteDML("INSERT INTO ProximityTypes VALUES (11, 'NoRadius', 0, 1, 1, 0, 1, 1);");
		auto& rows = CDClientManager::GetEntriesMutable<CDProximityMonitorComponentTable>();
		rows.push_back({ 3, "2,3", true, true });
		rows.push_back({ 6, "7,8", true, false });
		rows.push_back({ 50, "3,11,99", true, true });
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

// A rocket launch pad's "7,8" are the client's sign and launch proximities, not radii of 7 and 8: nothing for the server
TEST_F(ProximityMonitorComponentTests, ClientOnlyProximitiesAreNotMadeOnTheServer) {
	Entity launchpad(1, info);
	auto* const proximity = launchpad.AddComponent<ProximityMonitorComponent>(6);
	proximity->LoadDataFromTemplate();
	EXPECT_TRUE(proximity->GetProximitiesData().empty());
}

TEST_F(ProximityMonitorComponentTests, ServerProximitiesTakeTheTypesNameAndRadius) {
	Entity object(2, info);
	auto* const proximity = object.AddComponent<ProximityMonitorComponent>(3);
	proximity->LoadDataFromTemplate();
	const auto& made = proximity->GetProximitiesData();
	ASSERT_EQ(made.size(), 1u);
	ASSERT_TRUE(made.contains("Test Prox Large"));

	// A radius of 0 is 30, and a type that does not exist is skipped
	Entity other(3, info);
	auto* const otherProximity = other.AddComponent<ProximityMonitorComponent>(50);
	otherProximity->LoadDataFromTemplate();
	EXPECT_EQ(otherProximity->GetProximitiesData().size(), 2u);
	EXPECT_TRUE(otherProximity->GetProximitiesData().contains("NoRadius"));
}
