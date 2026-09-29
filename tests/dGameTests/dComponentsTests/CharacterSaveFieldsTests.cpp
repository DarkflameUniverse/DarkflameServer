// Charxml fields the client reads on load that DLU now saves. The formats are the ones live wrote (2011/2012 live
// captures of the charxml the world server sent) and the client reads (legouniverse.exe 1.10.64).
#include "GameDependencies.h"

#include "CDClientDatabase.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "Entity.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "eMissionLockState.h"

#include "tinyxml2.h"

#include <string>

#include <gtest/gtest.h>

namespace {
	std::string Print(const tinyxml2::XMLDocument& doc) {
		tinyxml2::XMLPrinter printer(nullptr, true);
		doc.Print(&printer);
		return printer.CStr();
	}

	void Parse(tinyxml2::XMLDocument& doc, const std::string& xml) {
		ASSERT_EQ(doc.Parse(xml.c_str()), tinyxml2::XML_SUCCESS);
	}
}

class CharacterSaveFieldsTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:"); // MissionComponent counts the achievements
		CDClientDatabase::ExecuteDML("CREATE TABLE Missions (id INTEGER, isMission INTEGER);");
	}
	void TearDown() override { TearDownDependencies(); }
};

// <mis><ts>: the states of the mission journal tabs, written after <cur> the way live wrote them.
TEST_F(CharacterSaveFieldsTests, MissionTypeStatesRoundTrip) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mis><done/><cur/><ts><type v="Build"><st sub="" val="1"/></type><type v="Location"><st sub="Avant Gardens" val="1"/><st sub="Nimbus Station" val="2"/></type></ts></mis></obj>)");

	Entity player(20, info);
	auto* const missions = player.AddComponent<MissionComponent>(-1);
	missions->LoadFromXml(doc);
	const auto& states = missions->GetMissionTypeStates();
	ASSERT_EQ(states.size(), 2);
	EXPECT_EQ(states.at("Build").at(""), eMissionLockState::NEW);
	EXPECT_EQ(states.at("Location").at("Avant Gardens"), eMissionLockState::NEW);
	EXPECT_EQ(states.at("Location").at("Nimbus Station"), eMissionLockState::UNLOCKED);

	missions->SetMissionTypeState("Battle", "General", eMissionLockState::NEW);
	missions->UpdateXml(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><mis><done/><cur/><ts><type v="Battle"><st sub="General" val="1"/></type><type v="Build"><st sub="" val="1"/></type><type v="Location"><st sub="Avant Gardens" val="1"/><st sub="Nimbus Station" val="2"/></type></ts></mis></obj>)");

	Entity reloaded(21, info);
	auto* const reloadedMissions = reloaded.AddComponent<MissionComponent>(-1);
	reloadedMissions->LoadFromXml(doc);
	EXPECT_EQ(reloadedMissions->GetMissionTypeStates(), states);
}

// Saves from before <ts> was written load with no states and gain an empty <ts>.
TEST_F(CharacterSaveFieldsTests, MissionTypeStatesMissingInOldSave) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mis><done/><cur/></mis></obj>)");

	Entity player(22, info);
	auto* const missions = player.AddComponent<MissionComponent>(-1);
	missions->LoadFromXml(doc);
	EXPECT_TRUE(missions->GetMissionTypeStates().empty());
	missions->UpdateXml(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><mis><done/><cur/><ts/></mis></obj>)");
}

// char@ttip: the tooltip bits (the client reads it with GetLongLongValue). Live wrote it on every character.
TEST_F(CharacterSaveFieldsTests, TooltipFlagsRoundTrip) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mf/><char ls="0" ttip="16777216"/></obj>)");

	Character character(1, nullptr);
	Entity player(23, info);
	auto* const characterComponent = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	characterComponent->LoadFromXml(doc);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 16777216u);

	characterComponent->SetTooltipFlag(2, true);
	characterComponent->UpdateXml(doc);
	EXPECT_STREQ(doc.FirstChildElement("obj")->FirstChildElement("char")->Attribute("ttip"), "16777220");

	Entity reloaded(24, info);
	auto* const reloadedComponent = reloaded.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	reloadedComponent->LoadFromXml(doc);
	EXPECT_EQ(reloadedComponent->GetTooltipFlags(), 16777220u);
}

// Saves from before ttip was written load with no tooltips flagged and gain ttip="0".
TEST_F(CharacterSaveFieldsTests, TooltipFlagsMissingInOldSave) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mf/><char ls="0"/></obj>)");

	Character character(1, nullptr);
	Entity player(25, info);
	auto* const characterComponent = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	characterComponent->LoadFromXml(doc);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 0u);
	characterComponent->UpdateXml(doc);
	EXPECT_STREQ(doc.FirstChildElement("obj")->FirstChildElement("char")->Attribute("ttip"), "0");
}
