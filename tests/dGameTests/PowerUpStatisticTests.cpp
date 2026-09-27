#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <map>

#include "GameDependencies.h"
#include "CharacterComponent.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDObjectSkillsTable.h"
#include "CDSkillBehaviorTable.h"
#include "CDBehaviorTemplateTable.h"
#include "CDBehaviorParameterTable.h"

// Which statistic a picked up power-up counts towards comes from what its pickup skill restores.

namespace {
	void LoadTables() {
		CDClientManager::GetTable<CDObjectSkillsTable>()->LoadValuesFromDatabase();
		CDClientManager::GetTable<CDSkillBehaviorTable>()->LoadValuesFromDatabase();
		CDClientManager::GetTable<CDBehaviorTemplateTable>()->LoadValuesFromDatabase();
		CDClientManager::GetTable<CDBehaviorParameterTable>()->LoadValuesFromDatabase();
	}
}

class PowerUpStatisticFixtureTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:");
		for (const auto* sql : {
			"CREATE TABLE Objects (id INTEGER, name TEXT, placeable INTEGER, type TEXT, description TEXT, localize INTEGER, npcTemplateID INTEGER, displayName TEXT, interactionDistance REAL, nametag INTEGER, _internalNotes TEXT, locStatus INTEGER, gate_version TEXT, HQ_valid INTEGER);",
			"CREATE TABLE ObjectSkills (objectTemplate INTEGER, skillID INTEGER, castOnType INTEGER, AICombatWeight INTEGER);",
			"CREATE TABLE SkillBehavior (skillID INTEGER, locStatus INTEGER, behaviorID INTEGER, imaginationcost INTEGER, cooldowngroup INTEGER, cooldown REAL, inNpcEditor INTEGER, skillIcon INTEGER, oomSkillID TEXT, oomBehaviorEffectID INTEGER, castTypeDesc INTEGER, imBonusUI INTEGER, lifeBonusUI INTEGER, armorBonusUI INTEGER, damageUI INTEGER, hideIcon INTEGER, localize INTEGER, gate_version TEXT, cancelType INTEGER);",
			"CREATE TABLE BehaviorTemplate (behaviorID INTEGER, templateID INTEGER, effectID INTEGER, effectHandle TEXT);",
			"CREATE TABLE BehaviorParameter (behaviorID INTEGER, parameterID TEXT, value REAL);",
			// 90501: a power-up whose skill is an And of a buff removal and armor repair; 90502: same skill, not a power-up;
			// 90503: a power-up whose skill restores nothing
			"INSERT INTO Objects (id, name, type) VALUES (90501, 'Armor Powerup', 'Powerup'), (90502, 'Not a powerup', 'Environmental'), (90503, 'Speed Powerup', 'Powerup');",
			"INSERT INTO ObjectSkills VALUES (90501, 90601, 2, 0), (90502, 90601, 2, 0), (90503, 90602, 2, 0);",
			"INSERT INTO SkillBehavior (skillID, behaviorID, imaginationcost, cooldowngroup, cooldown, cancelType) VALUES (90601, 900101, 0, 0, 0, 0), (90602, 900104, 0, 0, 0, 0);",
			// 900101 And -> 900102 RemoveBuff, 900103 RepairArmor; 900104 Speed
			"INSERT INTO BehaviorTemplate VALUES (900101, 18, 0, ''), (900102, 55, 0, ''), (900103, 22, 0, ''), (900104, 37, 0, '');",
			"INSERT INTO BehaviorParameter VALUES (900101, 'behavior 1', 900102), (900101, 'behavior 2', 900103), (900103, 'armor', 1), (900104, 'run_speed', 900103);",
			}) {
			CDClientDatabase::ExecuteDML(sql);
		}
		LoadTables();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(PowerUpStatisticFixtureTest, StatisticFollowsThePickupSkill) {
	EXPECT_EQ(CharacterComponent::GetPowerUpStatistic(90501), std::optional<StatisticID>(ArmorPowerUpsCollected));
	EXPECT_EQ(CharacterComponent::GetPowerUpStatistic(90502), std::nullopt);
	// Only behavior parameters ("action", "behavior N") are followed, not other values that happen to be behavior IDs
	EXPECT_EQ(CharacterComponent::GetPowerUpStatistic(90503), std::nullopt);
	EXPECT_EQ(CharacterComponent::GetPowerUpStatistic(90504), std::nullopt);
}

// Against the live CDClient, when there is one (DLU_CDSERVER_SQLITE, or build/resServer/CDServer.sqlite).
class PowerUpStatisticLiveDataTest : public GameDependenciesTest {
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

TEST_F(PowerUpStatisticLiveDataTest, MatchesTheOldHardcodedPowerUps) {
	const std::map<LOT, StatisticID> expected = {
		{ 935, ImaginationPowerUpsCollected }, { 4035, ImaginationPowerUpsCollected }, { 11910, ImaginationPowerUpsCollected },
		{ 11911, ImaginationPowerUpsCollected }, { 11918, ImaginationPowerUpsCollected },
		{ 6431, ArmorPowerUpsCollected }, { 11912, ArmorPowerUpsCollected }, { 11913, ArmorPowerUpsCollected },
		{ 11914, ArmorPowerUpsCollected }, { 11919, ArmorPowerUpsCollected },
		{ 177, LifePowerUpsCollected }, { 11915, LifePowerUpsCollected }, { 11916, LifePowerUpsCollected },
		{ 11917, LifePowerUpsCollected }, { 11920, LifePowerUpsCollected },
	};
	for (const auto& [lot, statistic] : expected) {
		EXPECT_EQ(CharacterComponent::GetPowerUpStatistic(lot), std::optional<StatisticID>(statistic)) << "LOT " << lot;
	}

	// Not power-ups, or power-ups that restore nothing
	for (const LOT lot : { 6253, 7230, 8200, 13763 }) {
		EXPECT_EQ(CharacterComponent::GetPowerUpStatistic(lot), std::nullopt) << "LOT " << lot;
	}
}
