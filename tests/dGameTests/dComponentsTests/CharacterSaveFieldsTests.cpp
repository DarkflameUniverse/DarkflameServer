// Charxml fields the client reads on load that DLU now saves. The formats are the ones live wrote (2011/2012 live
// captures of the charxml the world server sent) and the client reads (legouniverse.exe 1.10.64).
#include "GameDependencies.h"

#include "CDClientDatabase.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "Entity.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "SkillComponent.h"
#include "InventoryComponent.h"
#include "DatabasePet.h"
#include "CDSkillBehaviorTable.h"
#include "CDClientManager.h"
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

// <skil sc>: the cooldown groups still running and the seconds left, as live wrote them (sc="17:15.6958;") and the
// client reads them (split on ';' and ':').
TEST_F(CharacterSaveFieldsTests, SkillCooldownsRoundTrip) {
	CDSkillBehavior skill{};
	skill.skillID = 394;
	skill.cooldowngroup = 17;
	skill.cooldown = 30.0f;
	CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>()[394] = skill;
	CDSkillBehavior ungrouped{};
	ungrouped.skillID = 395;
	ungrouped.cooldowngroup = static_cast<uint32_t>(-1);
	ungrouped.cooldown = 30.0f;
	CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>()[395] = ungrouped;

	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><skil sc="8:26.6559;78:30.7363;"/></obj>)");

	Entity player(26, info);
	auto* const skills = player.AddComponent<SkillComponent>(-1);
	skills->LoadFromXml(doc);
	ASSERT_EQ(skills->GetCooldownGroups().size(), 2u);
	EXPECT_FLOAT_EQ(skills->GetCooldownGroups().at(8), 26.6559f);

	skills->Update(26.7f); // group 8 runs out
	skills->StartCooldown(394);
	skills->StartCooldown(395); // no group: not saved
	skills->UpdateXml(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><skil sc="17:30;78:4.0363;"/></obj>)");

	Entity reloaded(27, info);
	auto* const reloadedSkills = reloaded.AddComponent<SkillComponent>(-1);
	reloadedSkills->LoadFromXml(doc);
	EXPECT_EQ(reloadedSkills->GetCooldownGroups().size(), 2u);
	EXPECT_FLOAT_EQ(reloadedSkills->GetCooldownGroups().at(17), 30.0f);

	CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>().clear();
}

// Saves without <skil> load with no cooldowns and gain <skil/>, as live wrote it with none running.
TEST_F(CharacterSaveFieldsTests, SkillCooldownsMissingInOldSave) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"/>)");

	Entity player(28, info);
	auto* const skills = player.AddComponent<SkillComponent>(-1);
	skills->LoadFromXml(doc);
	EXPECT_TRUE(skills->GetCooldownGroups().empty());
	skills->UpdateXml(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><skil/></obj>)");
}

// <pet a="0">: live wrote the active pet as 0 on every character (the charxml is read at load, before any pet is out),
// and each pet's taming type t as 0. Old saves without a load as before.
TEST_F(CharacterSaveFieldsTests, PetsAsLiveWroteThem) {
	CDClientDatabase::ExecuteDML("CREATE TABLE ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER);");
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><pet><p id="1152921510000000001" l="3254" t="0" n="Fluffy" m="2"/></pet></obj>)");

	Entity player(29, info);
	auto* const inventory = player.AddComponent<InventoryComponent>(-1);
	inventory->LoadXml(doc); // the pets load first; there is no <inv> here
	EXPECT_EQ(inventory->GetDatabasePet(1152921510000000001LL).lot, 3254);
	inventory->UpdateXml(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><pet a="0"><p id="1152921510000000001" l="3254" m="2" n="Fluffy" t="0"/></pet></obj>)");
}
