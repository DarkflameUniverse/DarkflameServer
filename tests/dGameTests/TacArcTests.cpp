#include <gtest/gtest.h>

#include "BitStream.h"
#include "GameDependencies.h"
#include "TacArcBehavior.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDBehaviorTemplateTable.h"
#include "CDBehaviorParameterTable.h"

// TacArc target lists as the client writes and reads them (TacArcBehavior::DoHit / DoUnserializeBS)
class TacArcTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

namespace {
	void WriteIds(RakNet::BitStream& stream, const std::vector<LWOOBJID>& ids) {
		stream.Write<uint32_t>(ids.size());
		for (const auto id : ids) stream.Write(id);
	}
}

TEST_F(TacArcTests, TargetsAreReadAscendingAndOnce) {
	// The client handles each listed target once, in ascending id order, and skips empty ids
	RakNet::BitStream stream;
	WriteIds(stream, { 30, 10, LWOOBJID_EMPTY, 20, 10 });
	stream.Write<uint8_t>(0xAB); // the actions' data follows

	std::set<LWOOBJID> targets;
	ASSERT_TRUE(TacArcBehavior::ReadTargets(stream, 100, targets));
	EXPECT_EQ(std::vector<LWOOBJID>(targets.begin(), targets.end()), (std::vector<LWOOBJID>{ 10, 20, 30 }));

	uint8_t next = 0;
	ASSERT_TRUE(stream.Read(next));
	EXPECT_EQ(next, 0xAB);
}

TEST_F(TacArcTests, TooManyTargetsAreRefused) {
	RakNet::BitStream stream;
	WriteIds(stream, { 1, 2, 3 });
	std::set<LWOOBJID> targets;
	EXPECT_FALSE(TacArcBehavior::ReadTargets(stream, 2, targets));
}

TEST_F(TacArcTests, CutShortDataIsRefused) {
	RakNet::BitStream stream;
	stream.Write<uint32_t>(2);
	stream.Write<LWOOBJID>(5);
	std::set<LWOOBJID> targets;
	EXPECT_FALSE(TacArcBehavior::ReadTargets(stream, 100, targets));
}

TEST_F(TacArcTests, TheClosestTargetsAreWrittenAscending) {
	// Closest first; the two closest are kept and written in ascending id order, as the client's DoHit does
	RakNet::BitStream stream;
	const auto order = TacArcBehavior::WriteTargets(stream, { 90, 40, 70 }, 2);
	EXPECT_EQ(std::vector<LWOOBJID>(order.begin(), order.end()), (std::vector<LWOOBJID>{ 40, 90 }));

	uint32_t count = 0;
	LWOOBJID first = 0;
	LWOOBJID second = 0;
	ASSERT_TRUE(stream.Read(count));
	ASSERT_TRUE(stream.Read(first));
	ASSERT_TRUE(stream.Read(second));
	EXPECT_EQ(count, 2u);
	EXPECT_EQ(first, 40);
	EXPECT_EQ(second, 90);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0);
}

TEST_F(TacArcTests, WrittenTargetsReadBack) {
	RakNet::BitStream stream;
	const auto written = TacArcBehavior::WriteTargets(stream, { 7, 3, 5 }, 100);
	std::set<LWOOBJID> read;
	ASSERT_TRUE(TacArcBehavior::ReadTargets(stream, 100, read));
	EXPECT_EQ(read, written);
}

namespace {
	using Candidate = TacArcBehavior::Candidate;

	std::vector<LWOOBJID> Order(const std::vector<Candidate>& candidates, const bool useAttackPriority, const float distanceWeight = 0.0f, const float angleWeight = 0.0f, const float maxRange = 10.0f) {
		return TacArcBehavior::OrderTargets(candidates, distanceWeight, angleWeight, maxRange, useAttackPriority);
	}
}

TEST_F(TacArcTests, NearestTargetsComeFirst) {
	EXPECT_EQ(Order({ { 1, 5.0f }, { 2, 1.0f }, { 3, 3.0f } }, false), (std::vector<LWOOBJID>{ 2, 3, 1 }));
}

TEST_F(TacArcTests, LowerAttackPriorityComesFirst) {
	// Priority 1 before 10 however far it is; within a priority the nearest first
	const std::vector<Candidate> candidates = {
		{ .id = 1, .distance = 1.0f, .attackPriority = 10 },
		{ .id = 2, .distance = 7.0f, .attackPriority = 1 },
		{ .id = 3, .distance = 2.0f, .attackPriority = 10 },
		{ .id = 4, .distance = 5.0f, .attackPriority = 1 },
		{ .id = 5, .distance = 0.5f, .attackPriority = 5 },
	};
	EXPECT_EQ(Order(candidates, true), (std::vector<LWOOBJID>{ 4, 2, 5, 1, 3 }));
}

TEST_F(TacArcTests, EnemiesComeBeforeSmashablesThroughTheirPriority) {
	// An enemy (attack_priority 1) behind a closer crate (10) takes a one-target swing
	auto ordered = Order({ { .id = 100, .distance = 1.0f, .attackPriority = 10 }, { .id = 200, .distance = 6.0f, .attackPriority = 1 } }, true);
	ordered.resize(1);
	EXPECT_EQ(ordered, (std::vector<LWOOBJID>{ 200 }));

	// A smashable that also has priority 1 competes with the enemy on distance only
	EXPECT_EQ(Order({ { .id = 100, .distance = 1.0f, .attackPriority = 1 }, { .id = 200, .distance = 6.0f, .attackPriority = 1 } }, true), (std::vector<LWOOBJID>{ 100, 200 }));
}

TEST_F(TacArcTests, AttackPriorityIsIgnoredWithoutTheFlag) {
	EXPECT_EQ(Order({ { .id = 100, .distance = 1.0f, .attackPriority = 10 }, { .id = 200, .distance = 6.0f, .attackPriority = 1 } }, false), (std::vector<LWOOBJID>{ 100, 200 }));
}

TEST_F(TacArcTests, AttackPriorityIsSigned) {
	EXPECT_EQ(Order({ { .id = 1, .distance = 1.0f, .attackPriority = 1 }, { .id = 2, .distance = 2.0f, .attackPriority = -1 } }, true), (std::vector<LWOOBJID>{ 2, 1 }));
}

TEST_F(TacArcTests, TiesKeepAscendingIds) {
	// Same distance and priority: the order the client's id set hands them over in
	EXPECT_EQ(Order({ { 30, 2.0f }, { 10, 2.0f }, { 20, 2.0f } }, true), (std::vector<LWOOBJID>{ 10, 20, 30 }));
	EXPECT_EQ(Order({ { 30, 2.0f }, { 10, 2.0f }, { 20, 1.0f } }, false), (std::vector<LWOOBJID>{ 20, 10, 30 }));
}

TEST_F(TacArcTests, WeightsRankByDistanceAndAngle) {
	// weight = distance_weight * (max range - distance) / max range + angle_weight * (180 - angle) / 180, heaviest first
	const std::vector<Candidate> candidates = {
		{ .id = 1, .distance = 2.0f, .angle = 90.0f }, // 0.8 + 0.5
		{ .id = 2, .distance = 8.0f, .angle = 0.0f },  // 0.2 + 1.0
		{ .id = 3, .distance = 7.0f, .angle = 36.0f }, // 0.3 + 0.8
	};
	EXPECT_EQ(Order(candidates, false, 1.0f, 1.0f), (std::vector<LWOOBJID>{ 1, 2, 3 }));
	// Only the angle counts: straight ahead first
	EXPECT_EQ(Order(candidates, false, 0.0f, 1.0f), (std::vector<LWOOBJID>{ 2, 3, 1 }));
	// The priority buckets keep the weighted order inside them
	auto withPriority = candidates;
	withPriority[1].attackPriority = 10;
	EXPECT_EQ(Order(withPriority, true, 0.0f, 1.0f), (std::vector<LWOOBJID>{ 3, 1, 2 }));
}

TEST_F(TacArcTests, KeptTargetsAreTheFirstOrderedAndWrittenAscending) {
	// Priority picks the enemies (ids 50, 60) over the nearer crates, then they are written in ascending id order
	const auto ordered = Order({
		{ .id = 60, .distance = 4.0f, .attackPriority = 1 },
		{ .id = 10, .distance = 1.0f, .attackPriority = 10 },
		{ .id = 50, .distance = 6.0f, .attackPriority = 1 },
		{ .id = 20, .distance = 2.0f, .attackPriority = 10 },
		}, true);
	RakNet::BitStream stream;
	const auto written = TacArcBehavior::WriteTargets(stream, ordered, 2);
	EXPECT_EQ(std::vector<LWOOBJID>(written.begin(), written.end()), (std::vector<LWOOBJID>{ 50, 60 }));
}

class TacArcParameterTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:");
		for (const auto* sql : {
			"CREATE TABLE BehaviorTemplate (behaviorID INTEGER, templateID INTEGER, effectID INTEGER, effectHandle TEXT);",
			"CREATE TABLE BehaviorParameter (behaviorID INTEGER, parameterID TEXT, value REAL);",
			// 990701 sets use_attack_priority, 990702 clears it, 990703 is an older TacArc without it
			"INSERT INTO BehaviorTemplate VALUES (990701, 3, 0, ''), (990702, 3, 0, ''), (990703, 3, 0, '');",
			"INSERT INTO BehaviorParameter VALUES (990701, 'use_attack_priority', 1), (990701, 'max range', 8), (990702, 'use_attack_priority', 0), (990702, 'max range', 8), (990703, 'max range', 8);",
			}) {
			CDClientDatabase::ExecuteDML(sql);
		}
		CDClientManager::GetTable<CDBehaviorTemplateTable>()->LoadValuesFromDatabase();
		CDClientManager::GetTable<CDBehaviorParameterTable>()->LoadValuesFromDatabase();
	}

	void TearDown() override { TearDownDependencies(); }
};

TEST_F(TacArcParameterTests, UseAttackPriorityIsOffUnlessSet) {
	// The client's TacArcBehavior::Initialize reads use_attack_priority with a default of 0
	TacArcBehavior set(990701);
	set.Load();
	TacArcBehavior cleared(990702);
	cleared.Load();
	TacArcBehavior missing(990703);
	missing.Load();
	EXPECT_TRUE(set.UsesAttackPriority());
	EXPECT_FALSE(cleared.UsesAttackPriority());
	EXPECT_FALSE(missing.UsesAttackPriority());
}
