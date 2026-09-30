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
#include "PetComponent.h"
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
	static constexpr LOT PET_BLOCKER_LOT = 3913; // PR - Pet Blocker: solid, group 18
	static constexpr int32_t PET_BLOCKER_PHYSICS = 1913;
	static constexpr LOT HEDGE_LOT = 3027; // AG - bush square section: solid, a navmesh carver in Robot City
	static constexpr int32_t HEDGE_PHYSICS = 90301;
	static constexpr int32_t PET_GROUP = 3;

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
		AddPhysics(PET_BLOCKER_LOT, eReplicaComponentType::SIMPLE_PHYSICS, PET_BLOCKER_PHYSICS, "miscellaneous\\misc_phys_10x1x5.hkx", 18);
		AddPhysics(HEDGE_LOT, eReplicaComponentType::SIMPLE_PHYSICS, HEDGE_PHYSICS, "env\\env_won_nim_bush_square-section.hkx", 7);
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
	}

	void TearDown() override {
		dpWorld::Shutdown(); // frees the blockers the world owns
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

// Pets are walked by the server too; the pet ranch's pet blockers keep them in (group 18 touches pets, group 3)
TEST_F(MovementBlockerScenarioTest, PetBlockerStopsAPet) {
	auto wall = Wall(PET_BLOCKER_LOT, 104);
	wall->AddComponent<SimplePhysicsComponent>(PET_BLOCKER_PHYSICS);
	ASSERT_EQ(dpWorld::GetMovementBlockers().size(), 1u);

	EntityInfo petInfo = info;
	petInfo.pos = { 0.0f, 0.0f, -10.0f };
	Entity pet(204, petInfo);
	pet.AddComponent<SimplePhysicsComponent>(-1)->SetCollisionGroup(PET_GROUP);
	pet.AddComponent<PetComponent>(-1);
	auto* const movement = pet.AddComponent<MovementAIComponent>(-1, MovementAIInfo{});
	movement->SetDestination({ 0.0f, 0.0f, 10.0f });
	EXPECT_LT(movement->GetDestination().z, -0.5f);
	EXPECT_GT(movement->GetDestination().z, -3.0f);
}

// A carver_only object is never spawned (the client never loads one), but it still stops the chase
TEST_F(MovementBlockerScenarioTest, CarverOnlyWallBlocksWithoutAnObject) {
	LwoNameValue settings;
	settings.Insert<bool>(u"carver_only", true);
	settings.Insert<bool>(u"navmesh_carver", true);
	EXPECT_TRUE(PhysicsComponent::IsCarverOnly(settings));
	ASSERT_TRUE(PhysicsComponent::AddLevelMovementBlocker(105, CARVER_LOT, settings, NiPoint3Constant::ZERO, QuatUtils::IDENTITY, 1.0f));
	ASSERT_EQ(dpWorld::GetMovementBlockers().size(), 1u);

	EntityInfo enemyInfo = info;
	enemyInfo.pos = { 0.0f, 0.0f, -10.0f };
	Entity enemy(205, enemyInfo);
	EXPECT_LT(ChaseAcross(enemy).z, -0.5f);

	dpWorld::Shutdown();
	EXPECT_TRUE(dpWorld::GetMovementBlockers().empty());
}

TEST_F(MovementBlockerScenarioTest, CarverOnlyNeedsItsFlagAndAWall) {
	LwoNameValue settings;
	EXPECT_FALSE(PhysicsComponent::IsCarverOnly(settings));
	settings.Insert<bool>(u"carver_only", false);
	EXPECT_FALSE(PhysicsComponent::IsCarverOnly(settings));
	// carver_only without navmesh_carver (most of them): nothing to block with
	settings.Insert<bool>(u"carver_only", true);
	EXPECT_TRUE(PhysicsComponent::IsCarverOnly(settings));
	EXPECT_FALSE(PhysicsComponent::AddLevelMovementBlocker(106, CARVER_LOT, settings, NiPoint3Constant::ZERO, QuatUtils::IDENTITY, 1.0f));
	EXPECT_TRUE(dpWorld::GetMovementBlockers().empty());
}

// The Robot City hedges carve the navmesh; their size comes from the client's collision shape (4.39 x 4.96 x 9.03)
TEST_F(MovementBlockerScenarioTest, HedgeHasItsRealSize) {
	auto hedge = Wall(HEDGE_LOT, 107);
	hedge->SetVar<bool>(u"navmesh_carver", true);
	hedge->AddComponent<SimplePhysicsComponent>(HEDGE_PHYSICS);
	const auto blockers = dpWorld::GetMovementBlockers();
	ASSERT_EQ(blockers.size(), 1u);
	const auto* const box = dynamic_cast<const dpShapeBox*>(blockers[0].entity->GetShape());
	ASSERT_NE(box, nullptr);
	EXPECT_NEAR(box->m_MaxX - box->m_MinX, 4.3949f, 1e-3f);
	EXPECT_NEAR(box->m_MaxY - box->m_MinY, 4.9645f, 1e-3f);
	EXPECT_NEAR(box->m_MaxZ - box->m_MinZ, 9.0341f, 1e-3f);
	// Centred where the shape is, a little off the object's position
	EXPECT_NEAR((box->m_MaxX + box->m_MinX) / 2.0f, -0.0343f, 1e-3f);
	EXPECT_NEAR((box->m_MaxZ + box->m_MinZ) / 2.0f, -0.0527f, 1e-3f);

	EntityInfo enemyInfo = info;
	enemyInfo.pos = { 0.0f, 0.0f, -10.0f };
	Entity enemy(206, enemyInfo);
	EXPECT_LT(ChaseAcross(enemy).z, -4.5f);
}
