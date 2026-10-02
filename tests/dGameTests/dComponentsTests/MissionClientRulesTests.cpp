// Mission rules the 1.10.64 client works by (LWOMissionComponent): the viewed flag, prerequisites, cooldowns and
// turning a mission in.
#include "GameDependencies.h"

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDMissionsTable.h"
#include "Entity.h"
#include "GameMessages.h"
#include "Mission.h"
#include "MissionComponent.h"
#include "MissionMessages.h"
#include "eMissionState.h"

#include "tinyxml2.h"

#include <gtest/gtest.h>

namespace {
	// Mission IDs only these tests use (the prerequisite cache is static)
	constexpr uint32_t VIEWED = 992001;
	constexpr uint32_t NOT_VIEWED = 992002;
	constexpr uint32_t FINISHED = 992003;

	CDMissions Row(const uint32_t id, const bool repeatable = false, const std::string& prereq = "", const int64_t cooldown = -1) {
		CDMissions row{};
		row.id = id;
		row.isMission = true;
		row.repeatable = repeatable;
		row.prereqMissionID = prereq;
		row.cooldownTime = cooldown;
		return row;
	}
}

class MissionClientRulesTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:"); // MissionComponent counts the achievements
		CDClientDatabase::ExecuteDML("CREATE TABLE Missions (id INTEGER, isMission INTEGER);");
		auto& missions = CDClientManager::GetEntriesMutable<CDMissionsTable>();
		for (const auto id : { VIEWED, NOT_VIEWED, FINISHED }) missions.push_back(Row(id));
	}

	void TearDown() override {
		CDClientManager::GetEntriesMutable<CDMissionsTable>().clear();
		TearDownDependencies();
	}

	static void Load(MissionComponent& missions, const std::string& done, const std::string& cur) {
		tinyxml2::XMLDocument doc;
		const auto xml = R"(<obj v="1"><mis><done>)" + done + "</done><cur>" + cur + "</cur></mis></obj>";
		ASSERT_EQ(doc.Parse(xml.c_str()), tinyxml2::XML_SUCCESS);
		missions.LoadFromSaveData(doc);
	}

	static std::string Save(MissionComponent& missions) {
		tinyxml2::XMLDocument doc;
		doc.Parse(R"(<obj v="1"/>)");
		missions.WriteSaveData(doc);
		tinyxml2::XMLPrinter printer(nullptr, true);
		doc.Print(&printer);
		return printer.CStr();
	}
};

TEST_F(MissionClientRulesTests, ViewedFlagIsSavedAndLoaded) {
	Entity player(42, info);
	auto* const missions = player.AddComponent<MissionComponent>(-1);
	// Saves from before the flag have no v; the client reads a number or "true"
	Load(*missions, R"(<m id="992003" state="8" cct="1" cts="1000"/>)",
		R"(<m id="992001" state="2" v="true"/><m id="992002" state="2"/>)");
	EXPECT_TRUE(missions->GetMission(VIEWED)->GetViewed());
	EXPECT_FALSE(missions->GetMission(NOT_VIEWED)->GetViewed());

	// SetMissionViewed (0x00bec7a0) only changes missions in progress
	GameMessages::SetMissionViewed message;
	message.missionID = NOT_VIEWED;
	message.viewed = true;
	message.Handle(player, UNASSIGNED_SYSTEM_ADDRESS);
	missions->SetMissionViewed(FINISHED, true);
	EXPECT_TRUE(missions->GetMission(NOT_VIEWED)->GetViewed());
	EXPECT_FALSE(missions->GetMission(FINISHED)->GetViewed());

	missions->SetMissionViewed(VIEWED, false);
	const auto saved = Save(*missions);
	EXPECT_NE(saved.find(R"(id="992002" v="1")"), std::string::npos) << saved;
	EXPECT_EQ(saved.find(R"(id="992001" v=)"), std::string::npos) << saved;
	const auto done = saved.substr(saved.find("<done>"), saved.find("<cur>") - saved.find("<done>"));
	EXPECT_EQ(done.find(" v="), std::string::npos) << saved;

	// And back
	Entity reloaded(43, info);
	auto* const again = reloaded.AddComponent<MissionComponent>(-1);
	Load(*again, "", saved.substr(saved.find("<cur>") + 5, saved.find("</cur>") - saved.find("<cur>") - 5));
	EXPECT_TRUE(again->GetMission(NOT_VIEWED)->GetViewed());
	EXPECT_FALSE(again->GetMission(VIEWED)->GetViewed());
}

TEST_F(MissionClientRulesTests, SetMissionViewedWire) {
	GameMessages::SetMissionViewed out;
	out.missionID = 1234;
	out.viewed = true;
	RakNet::BitStream stream;
	out.Serialize(stream);
	GameMessages::SetMissionViewed in;
	ASSERT_TRUE(in.Deserialize(stream));
	EXPECT_EQ(in.missionID, 1234);
	EXPECT_TRUE(in.viewed);
	EXPECT_EQ(stream.GetNumberOfBitsUsed(), 33);
}
