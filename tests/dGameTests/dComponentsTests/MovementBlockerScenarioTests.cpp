#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "GameMessages.h"
#include "BaseCombatAIComponent.h"
#include "CDClientDatabase.h"
#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"
#include "dpEntity.h"
#include "dpShapeBox.h"
#include "dpWorld.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "MovementAIComponent.h"
#include "PhantomPhysicsComponent.h"
#include "SimplePhysicsComponent.h"

// An enemy chasing across a wall it can't cross stops in front of it, the way the walls are set up in the level files
class MovementBlockerScenarioTest : public GameDependenciesTest {
protected:
	static constexpr LOT ENEMY_BLOCKER_LOT = 9709; // FV - Enemy Blocking Volume: solid, group 18
	static constexpr LOT CARVER_LOT = 8419; // Trigger Wall: phantom, group 7, navmesh_carver on the Sentinel camp's
	static constexpr LOT CLEAR_THREAT_LOT = 13632; // Clear threat list Trigger Wall: phantom, group 18
	static constexpr int32_t ENEMY_BLOCKER_PHYSICS = 4237;
	static constexpr int32_t CARVER_PHYSICS = 3901;
	static constexpr int32_t CLEAR_THREAT_PHYSICS = 6691;

	void SetUp() override {
		SetUpDependencies();
		if (!CDClientDatabase::isConnected) {
			CDClientDatabase::Connect(":memory:");
			for (const auto* table : {
				"ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER)",
				"BaseCombatAIComponent (id INTEGER, aggroRadius REAL, tetherSpeed REAL, pursuitSpeed REAL, softTetherRadius REAL, hardTetherRadius REAL, minRoundLength REAL, maxRoundLength REAL, combatRoundLength REAL)",
				"ObjectSkills (objectTemplate INTEGER, skillID INTEGER, castOnType INTEGER, AICombatWeight INTEGER)",
				"SkillBehavior (skillID INTEGER, behaviorID INTEGER)",
				}) {
				CDClientDatabase::ExecuteDML(std::string("CREATE TABLE ") + table + ";");
			}
		}

		AddPhysics(ENEMY_BLOCKER_LOT, eReplicaComponentType::SIMPLE_PHYSICS, ENEMY_BLOCKER_PHYSICS, "miscellaneous\\misc_phys_10x1x5.hkx", 18);
		AddPhysics(CARVER_LOT, eReplicaComponentType::PHANTOM_PHYSICS, CARVER_PHYSICS, "miscellaneous\\misc_phys_10x1x5.hkx", 7);
		AddPhysics(CLEAR_THREAT_LOT, eReplicaComponentType::PHANTOM_PHYSICS, CLEAR_THREAT_PHYSICS, "test\\POI_trigger_wall.hkx", 18);
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
	}

	void TearDown() override {
		TearDownDependencies();
	}

	void AddPhysics(const LOT lot, const eReplicaComponentType type, const int32_t id, const std::string& asset, const int32_t group) {
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(type) << 32 | static_cast<uint64_t>(lot), id);
		registry.insert_or_assign(static_cast<uint64_t>(lot), 0);
		CDPhysicsComponent row{};
		row.id = id;
		row.physicsAsset = asset;
		row.collisionGroup = group;
		CDClientManager::GetEntriesMutable<CDPhysicsComponentTable>().insert_or_assign(static_cast<uint32_t>(id), row);
	}

	// A wall across z = 0 at the origin, 10 wide along x
	std::unique_ptr<Entity> Wall(const LOT lot, const LWOOBJID id) {
		EntityInfo wallInfo = info;
		wallInfo.lot = lot;
		wallInfo.pos = NiPoint3Constant::ZERO;
		return std::make_unique<Entity>(id, wallInfo);
	}

	// An enemy 10 in front of the wall, chasing something 10 behind it
	NiPoint3 ChaseAcross(Entity& enemy) {
		// Something to hold its position, as its controllable physics would
		enemy.AddComponent<SimplePhysicsComponent>(-1);
		enemy.AddComponent<BaseCombatAIComponent>(-1);
		auto* const movement = enemy.AddComponent<MovementAIComponent>(-1, MovementAIInfo{});
		movement->SetDestination({ 0.0f, 0.0f, 10.0f });
		return movement->GetDestination();
	}
};

TEST_F(MovementBlockerScenarioTest, EnemyOnlyBlockerStopsTheChase) {
	auto wall = Wall(ENEMY_BLOCKER_LOT, 100);
	wall->AddComponent<SimplePhysicsComponent>(ENEMY_BLOCKER_PHYSICS);
	ASSERT_EQ(dpWorld::GetMovementBlockers().size(), 1u);

	EntityInfo enemyInfo = info;
	enemyInfo.pos = { 0.0f, 0.0f, -10.0f };
	Entity enemy(200, enemyInfo);
	const auto destination = ChaseAcross(enemy);
	// The wall's near face is z = -0.5
	EXPECT_LT(destination.z, -0.5f);
	EXPECT_GT(destination.z, -3.0f);

	wall.reset();
	EXPECT_TRUE(dpWorld::GetMovementBlockers().empty());
}

TEST_F(MovementBlockerScenarioTest, NavmeshCarverStopsTheChase) {
	auto wall = Wall(CARVER_LOT, 101);
	wall->SetVar<bool>(u"navmesh_carver", true);
	wall->AddComponent<PhantomPhysicsComponent>(CARVER_PHYSICS);
	ASSERT_EQ(dpWorld::GetMovementBlockers().size(), 1u);

	EntityInfo enemyInfo = info;
	enemyInfo.pos = { 0.0f, 0.0f, -10.0f };
	Entity enemy(201, enemyInfo);
	EXPECT_LT(ChaseAcross(enemy).z, -0.5f);
}

TEST_F(MovementBlockerScenarioTest, ClearThreatWallIsATriggerNotAWall) {
	auto wall = Wall(CLEAR_THREAT_LOT, 102);
	auto* const phantom = wall->AddComponent<PhantomPhysicsComponent>(CLEAR_THREAT_PHYSICS);
	EXPECT_TRUE(dpWorld::GetMovementBlockers().empty());
	// Its trigger is the wall's real shape now, not a stand in cube
	ASSERT_NE(phantom->GetdpEntity(), nullptr);
	const auto* const box = dynamic_cast<const dpShapeBox*>(phantom->GetdpEntity()->GetShape());
	ASSERT_NE(box, nullptr);
	EXPECT_NEAR(box->m_MaxY - box->m_MinY, 12.9755f, 1e-3f);

	EntityInfo enemyInfo = info;
	enemyInfo.pos = { 0.0f, 0.0f, -10.0f };
	Entity enemy(202, enemyInfo);
	EXPECT_NEAR(ChaseAcross(enemy).z, 10.0f, 1e-3f);
}

TEST_F(MovementBlockerScenarioTest, NonEnemyMoverIsNotClamped) {
	auto wall = Wall(ENEMY_BLOCKER_LOT, 103);
	wall->AddComponent<SimplePhysicsComponent>(ENEMY_BLOCKER_PHYSICS);

	EntityInfo npcInfo = info;
	npcInfo.pos = { 0.0f, 0.0f, -10.0f };
	Entity npc(203, npcInfo);
	npc.AddComponent<SimplePhysicsComponent>(-1);
	auto* const movement = npc.AddComponent<MovementAIComponent>(-1, MovementAIInfo{});
	movement->SetDestination({ 0.0f, 0.0f, 10.0f });
	EXPECT_NEAR(movement->GetDestination().z, 10.0f, 1e-3f);
}
