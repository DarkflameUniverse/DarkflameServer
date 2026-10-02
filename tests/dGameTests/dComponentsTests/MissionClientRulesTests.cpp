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
#include "MissionPrerequisites.h"
#include "eMissionState.h"

#include "tinyxml2.h"

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

namespace {
	// Mission IDs only these tests use (the prerequisite cache is static)
	constexpr uint32_t VIEWED = 992001;
	constexpr uint32_t NOT_VIEWED = 992002;
	constexpr uint32_t FINISHED = 992003;
	constexpr uint32_t REPEAT = 992004;       // repeatable, completed once, repeated now
	constexpr uint32_t AFTER_REPEAT = 992005; // needs REPEAT
	constexpr uint32_t DAILY = 992006;        // repeatable, completed, no cooldown
	constexpr uint32_t AFTER_DAILY = 992007;  // needs DAILY:9 (completed and available again)
	constexpr uint32_t AFTER_ACTIVE = 992008; // needs NOT_VIEWED:2 (in progress)
	constexpr uint32_t GROUPED = 992009;      // needs (FINISHED|VIEWED)&(992010|992011)
	constexpr uint32_t ZERO = 992012;         // prerequisite "0"

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
		for (const auto id : { VIEWED, NOT_VIEWED, FINISHED, 992010u, 992011u }) missions.push_back(Row(id));
		missions.push_back(Row(REPEAT, true));
		missions.push_back(Row(AFTER_REPEAT, false, std::to_string(REPEAT)));
		missions.push_back(Row(DAILY, true, "", 0));
		missions.push_back(Row(AFTER_DAILY, false, std::to_string(DAILY) + ":9"));
		missions.push_back(Row(AFTER_ACTIVE, false, std::to_string(NOT_VIEWED) + ":2"));
		missions.push_back(Row(GROUPED, false, "(992003|992001)&(992010|992011)"));
		missions.push_back(Row(ZERO, false, "0"));
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

namespace {
	bool Eval(const std::string& expression, const std::set<std::string>& met, std::vector<std::string>* asked = nullptr) {
		return MissionPrerequisites::EvaluateExpression(expression, [&](const std::string_view operand) {
			if (asked) asked->emplace_back(operand);
			return met.contains(std::string(operand));
		});
	}
}

TEST_F(MissionClientRulesTests, ExpressionsReadLeftToRightAsTheClient) {
	// Groups are their own value (DLU read "(509|229)&(812|...)" as 509 | (229 & (812|...)))
	EXPECT_FALSE(Eval("(509|229)&(812|813|814|815)", { "509" }));
	EXPECT_TRUE(Eval("(509|229)&(812|813|814|815)", { "229", "814" }));
	EXPECT_TRUE(Eval("1203, (1685|1321|1448|1292)", { "1203", "1448" }));
	EXPECT_FALSE(Eval("1203, (1685|1321|1448|1292)", { "1448" }));
	// No precedence: a | b & c is (a | b) & c
	EXPECT_FALSE(Eval("1|2&3", { "1" }));
	EXPECT_TRUE(Eval("1|2&3", { "1", "3" }));
	EXPECT_TRUE(Eval("1;2", { "1", "2" }));
	EXPECT_TRUE(Eval("1^2", { "2" }));
	EXPECT_FALSE(Eval("1^2", { "1", "2" }));
	EXPECT_TRUE(Eval("!1", {}));
	EXPECT_TRUE(Eval("~1 & 2", { "2" }));
	EXPECT_TRUE(Eval("!(1|2)", {}));
	EXPECT_TRUE(Eval(" 1882:10 | 1882:2", { "1882:2" }));
	// An operand with no operator before it is ignored
	EXPECT_TRUE(Eval("1 2", { "1" }));
	EXPECT_FALSE(Eval("", { "1" }));
	// And/or stop once the result is known
	std::vector<std::string> asked;
	EXPECT_TRUE(Eval("1|2|3", { "1" }, &asked));
	EXPECT_EQ(asked, (std::vector<std::string>{ "1" }));
	asked.clear();
	EXPECT_FALSE(Eval("1&(2|3)", {}, &asked));
	EXPECT_EQ(asked, (std::vector<std::string>{ "1" }));
}

TEST_F(MissionClientRulesTests, PrerequisitesAsTheClient) {
	Entity player(44, info);
	auto* const missions = player.AddComponent<MissionComponent>(-1);
	Load(*missions,
		R"(<m id="992003" state="8" cct="1" cts="1000"/><m id="992004" state="10" cct="1" cts="1000"/><m id="992006" state="8" cct="1" cts="1000"/>)",
		R"(<m id="992004" state="10"/><m id="992002" state="2"/>)");

	// A plain ID is met once the mission was ever completed, also while it is being repeated (DLU wanted state 8)
	EXPECT_TRUE(missions->CanAccept(AFTER_REPEAT));
	// id:state is the client's state mask: 9 = completed and available again (DLU compared the mission's own state)
	EXPECT_EQ(missions->GetClientMissionState(DAILY), 9);
	EXPECT_TRUE(missions->CanAccept(AFTER_DAILY));
	EXPECT_TRUE(missions->CanAccept(AFTER_ACTIVE));
	EXPECT_FALSE(missions->CanAccept(GROUPED)); // FINISHED done, but neither of the second group
	EXPECT_TRUE(missions->CanAccept(ZERO));

	Entity other(45, info);
	auto* const fresh = other.AddComponent<MissionComponent>(-1);
	EXPECT_FALSE(fresh->CanAccept(AFTER_REPEAT));
	EXPECT_FALSE(fresh->CanAccept(AFTER_DAILY));
	EXPECT_FALSE(fresh->CanAccept(AFTER_ACTIVE));
	EXPECT_EQ(fresh->GetClientMissionState(AFTER_DAILY), 0);
}
