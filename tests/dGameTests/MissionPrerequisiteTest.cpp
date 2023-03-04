#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "MissionPrerequisites.h"
#include "eMissionState.h"

class MissionPrerequisiteTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

/**
 * Test that the structure of a single prerequisite mission is created correctly.
 */
TEST_F(MissionPrerequisiteTest, OneMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("1964");
	ASSERT_EQ(preReq->mission, 1964);
	delete preReq;
};

/**
 * Test that the structure of a multi mission prerequisite is created correctly.
 */
TEST_F(MissionPrerequisiteTest, MultiMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("1806,1868");
	ASSERT_EQ(preReq->mission, 1806);
	ASSERT_EQ(preReq->nextMission->mission, 1868);
	delete preReq;
};

/**
 * Test that the structure of an or prerequisite for missions is created properly
 */
TEST_F(MissionPrerequisiteTest, OrMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("2061|891");
	ASSERT_TRUE(preReq->m_Or);
	ASSERT_EQ(preReq->mission, 2061);
	ASSERT_EQ(preReq->nextMission->mission, 891);
	delete preReq;
};

/**
 * Test that the structure of an or prerequisite for multiple missions is created properly
 */
TEST_F(MissionPrerequisiteTest, MultiOrMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("815|812|813|814");
	ASSERT_TRUE(preReq->m_Or);
	ASSERT_EQ(preReq->mission, 815);
	ASSERT_EQ(preReq->nextMission->mission, 812);
	ASSERT_TRUE(preReq->nextMission->m_Or);
	ASSERT_EQ(preReq->nextMission->nextMission->mission, 813);
	ASSERT_TRUE(preReq->nextMission->nextMission->m_Or);
	ASSERT_EQ(preReq->nextMission->nextMission->nextMission->mission, 814);
	ASSERT_FALSE(preReq->nextMission->nextMission->nextMission->m_Or);
	ASSERT_EQ(preReq->nextMission->nextMission->nextMission->nextMission, nullptr);
	delete preReq;
};

/**
 * Test that the structure of a mission with an expected state is created properly
 */
TEST_F(MissionPrerequisiteTest, MissionStatePrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("236:2");
	ASSERT_EQ(preReq->mission, 236);
	ASSERT_EQ(preReq->missionState, eMissionState::ACTIVE);
	delete preReq;
};

/**
 * Test that the structure of an or prerequisite for missions is created properly
 */
TEST_F(MissionPrerequisiteTest, AndMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("(2061|100)&(891|1500)");
	ASSERT_EQ(preReq->mission, 0);
	ASSERT_FALSE(preReq->m_Or);

	ASSERT_EQ(preReq->nextMission->mission, 2061);
	ASSERT_TRUE(preReq->nextMission->m_Or);

	ASSERT_EQ(preReq->nextMission->nextMission->mission, 100);
	ASSERT_FALSE(preReq->nextMission->nextMission->m_Or);

	ASSERT_EQ(preReq->nextMission->nextMission->nextMission->mission, 0);
	ASSERT_FALSE(preReq->nextMission->nextMission->nextMission->m_Or);

	ASSERT_EQ(preReq->nextMission->nextMission->nextMission->nextMission->mission, 891);
	ASSERT_TRUE(preReq->nextMission->nextMission->nextMission->nextMission->m_Or);

	ASSERT_EQ(preReq->nextMission->nextMission->nextMission->nextMission->nextMission->mission, 1500);
	ASSERT_FALSE(preReq->nextMission->nextMission->nextMission->nextMission->nextMission->m_Or);

	ASSERT_EQ(preReq->nextMission->nextMission->nextMission->nextMission->nextMission->nextMission, nullptr);
	delete preReq;
};
