#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>

#include "GameDependencies.h"
#include "ItemSetPassiveAbility.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDScriptComponentTable.h"
#include "CDSkillBehaviorTable.h"
#include "CDBehaviorTemplateTable.h"

// Item set passive abilities come from the CDClient (DarkInspiration set skills) and the item
// scripts linked to the set items (equipmenttriggers). These tests check both against the values
// the server used to hardcode per set ID.

namespace {
	std::array<std::vector<uint32_t>, 5> ReadSkillSets(uint32_t setID) {
		std::array<std::vector<uint32_t>, 5> skillSets;
		auto query = CDClientDatabase::CreatePreppedStmt(
			"SELECT skillSetWith2, skillSetWith3, skillSetWith4, skillSetWith5, skillSetWith6 FROM ItemSets WHERE setID = ?;");
		query.bind(1, static_cast<int>(setID));
		auto result = query.execQuery();
		if (result.eof()) return skillSets;
		for (int i = 0; i < 5; ++i) {
			if (result.fieldIsNull(i)) continue;
			auto skillQuery = CDClientDatabase::CreatePreppedStmt("SELECT SkillID FROM ItemSetSkills WHERE SkillSetID = ?;");
			skillQuery.bind(1, result.getIntField(i));
			auto skills = skillQuery.execQuery();
			while (!skills.eof()) {
				skillSets[i].push_back(skills.getIntField(0));
				skills.nextRow();
			}
		}
		return skillSets;
	}

	std::vector<LOT> ReadItems(uint32_t setID) {
		std::vector<LOT> items;
		auto query = CDClientDatabase::CreatePreppedStmt("SELECT itemIDs FROM ItemSets WHERE setID = ?;");
		query.bind(1, static_cast<int>(setID));
		auto result = query.execQuery();
		if (result.eof()) return items;
		std::string ids = result.getStringField(0);
		std::string token;
		for (const char c : ids + ",") {
			if (c == ',') {
				if (!token.empty()) items.push_back(std::stoi(token));
				token.clear();
			} else if (!std::isspace(static_cast<unsigned char>(c))) {
				token += c;
			}
		}
		return items;
	}

	float Parameter(uint32_t behaviorID, const std::string& name) {
		auto query = CDClientDatabase::CreatePreppedStmt("SELECT value FROM BehaviorParameter WHERE behaviorID = ? AND parameterID = ?;");
		query.bind(1, static_cast<int>(behaviorID));
		query.bind(2, name.c_str());
		auto result = query.execQuery();
		return result.eof() ? -1.0f : static_cast<float>(result.getFloatField(0));
	}

	uint32_t Template(uint32_t behaviorID) {
		auto query = CDClientDatabase::CreatePreppedStmt("SELECT templateID FROM BehaviorTemplate WHERE behaviorID = ?;");
		query.bind(1, static_cast<int>(behaviorID));
		auto result = query.execQuery();
		return result.eof() ? 0 : result.getIntField(0);
	}

	// What a DarkInspiration on-kill behavior ends up doing to the wearer: "<template>=<amount>"
	std::string OnKillEffect(uint32_t behaviorID) {
		constexpr uint32_t TargetCaster = 14;
		constexpr uint32_t Heal = 5;
		constexpr uint32_t Imagination = 13;
		constexpr uint32_t RepairArmor = 22;
		if (Parameter(behaviorID, "faction_list") != 4.0f) return "wrong faction";
		const auto targetCaster = static_cast<uint32_t>(Parameter(behaviorID, "action"));
		if (Template(targetCaster) != TargetCaster) return "not target caster";
		const auto effect = static_cast<uint32_t>(Parameter(targetCaster, "action"));
		switch (Template(effect)) {
		case Heal: return "heal=" + std::to_string(static_cast<int>(Parameter(effect, "health")));
		case Imagination: return "imagination=" + std::to_string(static_cast<int>(Parameter(effect, "imagination")));
		case RepairArmor: return "armor=" + std::to_string(static_cast<int>(Parameter(effect, "armor")));
		default: return "template " + std::to_string(Template(effect));
		}
	}

	void LoadTables() {
		CDClientManager::GetTable<CDScriptComponentTable>()->LoadValuesFromDatabase();
		CDClientManager::GetTable<CDSkillBehaviorTable>()->LoadValuesFromDatabase();
		CDClientManager::GetTable<CDBehaviorTemplateTable>()->LoadValuesFromDatabase();
	}
}

TEST(ItemSetPassiveAbilityTests, StatTriggerScriptLookupIgnoresPathAndCase) {
	const auto* trigger = ItemSetPassiveAbility::GetStatTriggerForScript("scripts\\equipmenttriggers\\assemblyInventor2.lua");
	ASSERT_NE(trigger, nullptr);
	EXPECT_EQ(trigger->trigger, PassiveAbilityTrigger::AssemblyImagination);
	EXPECT_EQ(trigger->skillID, 581);
	EXPECT_EQ(trigger->itemsRequired, 4);
	EXPECT_EQ(trigger->setID, 26);
	EXPECT_FLOAT_EQ(trigger->cooldown, 11.0f);

	trigger = ItemSetPassiveAbility::GetStatTriggerForScript("scripts/equipmenttriggers/sentinelKnight1.lua");
	ASSERT_NE(trigger, nullptr);
	EXPECT_EQ(trigger->trigger, PassiveAbilityTrigger::SentinelArmor);
	EXPECT_EQ(trigger->skillID, 559);
	EXPECT_EQ(trigger->setID, 7);

	EXPECT_EQ(ItemSetPassiveAbility::GetStatTriggerForScript("scripts\\equipmenttriggers\\gempack.lua"), nullptr);
	EXPECT_EQ(ItemSetPassiveAbility::GetStatTriggerForScript(""), nullptr);
}

// A small CDClient: items 90001-90002 carry the Summoner rank 1 script, 90003 the Knight rank 1 script and
// 90004 no script; skills 90201 and 90203 are DarkInspiration, 90202 a buff.
class ItemSetPassiveAbilityFixtureTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:");
		for (const auto* sql : {
			"CREATE TABLE ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER);",
			"CREATE TABLE ScriptComponent (id INTEGER, script_name TEXT, client_script_name TEXT);",
			"CREATE TABLE SkillBehavior (skillID INTEGER, locStatus INTEGER, behaviorID INTEGER, imaginationcost INTEGER, cooldowngroup INTEGER, cooldown REAL, inNpcEditor INTEGER, skillIcon INTEGER, oomSkillID TEXT, oomBehaviorEffectID INTEGER, castTypeDesc INTEGER, imBonusUI INTEGER, lifeBonusUI INTEGER, armorBonusUI INTEGER, damageUI INTEGER, hideIcon INTEGER, localize INTEGER, gate_version TEXT, cancelType INTEGER);",
			"CREATE TABLE BehaviorTemplate (behaviorID INTEGER, templateID INTEGER, effectID INTEGER, effectHandle TEXT);",
			// Two items of the set carry the Summoner rank 1 script (set 28), one carries the Knight rank 1 script (set 7)
			"INSERT INTO ComponentsRegistry VALUES (90001, 5, 90101), (90002, 5, 90101), (90003, 5, 90102), (90004, 11, 1);",
			"INSERT INTO ScriptComponent VALUES (90101, 'scripts\\equipmenttriggers\\assemblysummoner1.lua', ''), (90102, 'scripts\\equipmenttriggers\\sentinelKnight1.lua', '');",
			"INSERT INTO SkillBehavior (skillID, behaviorID, imaginationcost, cooldowngroup, cooldown, cancelType) VALUES (90201, 900001, 0, 0, 0, 0), (90202, 900002, 0, 0, 0, 0), (90203, 900003, 0, 0, 0, 0);",
			// 24 = DarkInspiration, 30 = Buff
			"INSERT INTO BehaviorTemplate VALUES (900001, 24, 0, ''), (900002, 30, 0, ''), (900003, 24, 0, '');",
			}) {
			CDClientDatabase::ExecuteDML(sql);
		}
		LoadTables();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(ItemSetPassiveAbilityFixtureTest, StatTriggersComeFromTheItemScriptsOfThatSet) {
	const std::vector<LOT> items = { 90001, 90002, 90003, 90004 };

	const auto summoner = ItemSetPassiveAbility::FindStatTriggers(28, items);
	ASSERT_EQ(summoner.size(), 1); // two items carry the script, the skill is cast once
	EXPECT_EQ(summoner[0].trigger, PassiveAbilityTrigger::AssemblyImagination);
	EXPECT_EQ(summoner[0].skillID, 394);
	EXPECT_EQ(summoner[0].itemsRequired, 4);
	EXPECT_FLOAT_EQ(summoner[0].cooldown, 11.0f);

	const auto knight = ItemSetPassiveAbility::FindStatTriggers(7, items);
	ASSERT_EQ(knight.size(), 1);
	EXPECT_EQ(knight[0].trigger, PassiveAbilityTrigger::SentinelArmor);
	EXPECT_EQ(knight[0].skillID, 559);

	// The scripts only fire for the set they were written for
	EXPECT_TRUE(ItemSetPassiveAbility::FindStatTriggers(9001, items).empty());
}

TEST_F(ItemSetPassiveAbilityFixtureTest, OnKillSkillsAreTheDarkInspirationSetSkills) {
	std::array<std::vector<uint32_t>, 5> skillSets;
	skillSets[2] = { 90202, 90201 }; // with4: a buff and an on-kill skill
	skillSets[3] = { 90201, 90203 }; // with5: the same on-kill skill again and a second one
	skillSets[4] = { 99999 }; // with6: a skill that is not in SkillBehavior

	const auto onKill = ItemSetPassiveAbility::FindOnKillSkills(skillSets);
	ASSERT_EQ(onKill.size(), 2);
	EXPECT_EQ(onKill[0].skillID, 90201);
	EXPECT_EQ(onKill[0].behaviorID, 900001);
	EXPECT_EQ(onKill[0].itemsRequired, 4);
	EXPECT_EQ(onKill[1].skillID, 90203);
	EXPECT_EQ(onKill[1].behaviorID, 900003);
	EXPECT_EQ(onKill[1].itemsRequired, 5);

	EXPECT_TRUE(ItemSetPassiveAbility::FindOnKillSkills({}).empty());
}

// Against the live CDClient, when there is one (DLU_CDSERVER_SQLITE, or build/resServer/CDServer.sqlite).
class ItemSetPassiveAbilityLiveDataTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		std::string path;
		if (const char* env = std::getenv("DLU_CDSERVER_SQLITE")) path = env;
		else path = std::string(PROJECT_SOURCE_DIR) + "/build/resServer/CDServer.sqlite";
		if (!std::filesystem::exists(path)) GTEST_SKIP() << "No CDServer.sqlite at " << path;

		SetUpDependencies();
		CDClientDatabase::Connect(path);
		CDClientDatabase::ExecuteDML("PRAGMA query_only = ON;"); // never write to the live CDClient
		LoadTables();
	}

	void TearDown() override {
		if (!IsSkipped()) TearDownDependencies();
	}
};

TEST_F(ItemSetPassiveAbilityLiveDataTest, StatTriggersMatchTheOldHardcodedSets) {
	// set ID -> (trigger, skill) the server used to hardcode, all at 4 items
	const std::map<uint32_t, std::pair<PassiveAbilityTrigger, uint32_t>> expected = {
		{ 2, { PassiveAbilityTrigger::AssemblyImagination, 394 } }, { 25, { PassiveAbilityTrigger::AssemblyImagination, 394 } }, { 28, { PassiveAbilityTrigger::AssemblyImagination, 394 } },
		{ 3, { PassiveAbilityTrigger::AssemblyImagination, 581 } }, { 26, { PassiveAbilityTrigger::AssemblyImagination, 581 } }, { 29, { PassiveAbilityTrigger::AssemblyImagination, 581 } },
		{ 4, { PassiveAbilityTrigger::AssemblyImagination, 582 } }, { 27, { PassiveAbilityTrigger::AssemblyImagination, 582 } }, { 30, { PassiveAbilityTrigger::AssemblyImagination, 582 } },
		{ 7, { PassiveAbilityTrigger::SentinelArmor, 559 } }, { 8, { PassiveAbilityTrigger::SentinelArmor, 560 } }, { 9, { PassiveAbilityTrigger::SentinelArmor, 561 } },
		{ 10, { PassiveAbilityTrigger::SentinelArmor, 1101 } }, { 11, { PassiveAbilityTrigger::SentinelArmor, 1102 } }, { 12, { PassiveAbilityTrigger::SentinelArmor, 1103 } },
		{ 13, { PassiveAbilityTrigger::SentinelArmor, 562 } }, { 14, { PassiveAbilityTrigger::SentinelArmor, 563 } }, { 15, { PassiveAbilityTrigger::SentinelArmor, 564 } },
	};

	for (uint32_t setID = 1; setID <= 60; ++setID) {
		const auto triggers = ItemSetPassiveAbility::FindStatTriggers(setID, ReadItems(setID));
		const auto it = expected.find(setID);
		if (it == expected.end()) {
			EXPECT_TRUE(triggers.empty()) << "set " << setID;
			continue;
		}
		ASSERT_EQ(triggers.size(), 1) << "set " << setID;
		EXPECT_EQ(triggers[0].trigger, it->second.first) << "set " << setID;
		EXPECT_EQ(triggers[0].skillID, it->second.second) << "set " << setID;
		EXPECT_EQ(triggers[0].itemsRequired, 4) << "set " << setID;
		// The faction (Assembly) scripts have an 11 second cooldown, the Sentinel scripts none
		EXPECT_FLOAT_EQ(triggers[0].cooldown, it->second.first == PassiveAbilityTrigger::AssemblyImagination ? 11.0f : 0.0f) << "set " << setID;
	}
}

TEST_F(ItemSetPassiveAbilityLiveDataTest, OnKillSkillsMatchTheOldHardcodedSets) {
	// set ID -> (items, effect) the server used to hardcode on enemy smash
	const std::map<uint32_t, std::pair<uint32_t, std::string>> expected = {
		{ 42, { 5, "heal=3" } }, // Bat Lord
		{ 8, { 5, "armor=1" } }, { 9, { 5, "armor=1" } }, // Knight
		{ 11, { 5, "armor=1" } }, { 12, { 5, "armor=1" } }, // Space Ranger
		{ 14, { 5, "armor=1" } }, { 15, { 5, "armor=1" } }, // Samurai
		{ 16, { 4, "imagination=1" } }, { 17, { 4, "imagination=2" } }, { 18, { 4, "imagination=3" } }, // Sorcerer
		{ 19, { 4, "imagination=1" } }, { 20, { 4, "imagination=2" } }, { 21, { 4, "imagination=3" } }, // Space Marauder
		{ 22, { 4, "imagination=1" } }, { 23, { 4, "imagination=2" } }, { 24, { 4, "imagination=3" } }, // Shinobi
	};

	for (const auto& [setID, expectation] : expected) {
		const auto onKill = ItemSetPassiveAbility::FindOnKillSkills(ReadSkillSets(setID));
		ASSERT_FALSE(onKill.empty()) << "set " << setID;
		// The first on-kill skill is the one the server hardcoded; the data can add more
		// (e.g. the rank 2 and 3 Paradox team imagination at 4 and 5 items)
		EXPECT_EQ(onKill[0].itemsRequired, expectation.first) << "set " << setID;
		EXPECT_EQ(OnKillEffect(onKill[0].behaviorID), expectation.second) << "set " << setID;
	}

	// The rank 1 Sentinel sets were hardcoded to repair armor at 5 items, but only have 4 items
	for (const uint32_t setID : { 7, 10, 13 }) {
		EXPECT_LT(ReadItems(setID).size(), 5) << "set " << setID;
		EXPECT_TRUE(ItemSetPassiveAbility::FindOnKillSkills(ReadSkillSets(setID)).empty()) << "set " << setID;
	}
}
