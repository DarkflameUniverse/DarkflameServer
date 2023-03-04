#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "MissionPrerequisites.h"
#include "eMissionState.h"

class MissionPrerequisiteTest : public GameDependenciesTest {
protected:
	std::unordered_map<uint32_t, Mission*> missionMap;
	void SetUp() override {
		SetUpDependencies();
	}

	void TearDown() override {
		TearDownDependencies();
		for (auto mission : missionMap) {
			if (mission.second) delete mission.second;
		}
	}
};

/**
 * Test that the structure of a single prerequisite mission is created correctly.
 */
TEST_F(MissionPrerequisiteTest, OneMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("1964");
	ASSERT_EQ(preReq->a, 1964);
	missionMap[1964] = new Mission();
	missionMap[1] = new Mission();
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[1964]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	delete preReq;
};

/**
 * Test that the structure of a multi mission prerequisite is created correctly.
 */
TEST_F(MissionPrerequisiteTest, MultiMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("1806,1868");
	missionMap[1806] = new Mission();
	missionMap[1868] = new Mission();
	ASSERT_EQ(preReq->a, 1806);
	ASSERT_FALSE(preReq->m_or);
	ASSERT_EQ(preReq->b->a, 1868);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[1806]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[1806]->SetMissionState(eMissionState::ACTIVE);
	missionMap[1868]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[1806]->SetMissionState(eMissionState::COMPLETE);
	missionMap[1868]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	delete preReq;
};

/**
 * Test that the structure of an or prerequisite for missions is created properly
 */
TEST_F(MissionPrerequisiteTest, OrMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("2061|891");
	missionMap[2061] = new Mission();
	missionMap[891] = new Mission();
	ASSERT_TRUE(preReq->m_or);
	ASSERT_EQ(preReq->a, 2061);
	ASSERT_EQ(preReq->b->a, 891);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[2061]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	missionMap[2061]->SetMissionState(eMissionState::ACTIVE);
	missionMap[891]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	missionMap[2061]->SetMissionState(eMissionState::COMPLETE);
	missionMap[891]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	delete preReq;
};

/**
 * Test that the structure of an or prerequisite for multiple missions is created properly
 */
TEST_F(MissionPrerequisiteTest, MultiOrMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("815|812|813|814");
	missionMap[815] = new Mission();
	missionMap[812] = new Mission();
	missionMap[813] = new Mission();
	missionMap[814] = new Mission();
	ASSERT_TRUE(preReq->m_or);
	ASSERT_EQ(preReq->a, 815);
	ASSERT_EQ(preReq->b->a, 812);
	ASSERT_TRUE(preReq->b->m_or);
	ASSERT_EQ(preReq->b->b->a, 813);
	ASSERT_TRUE(preReq->b->b->m_or);
	ASSERT_EQ(preReq->b->b->b->a, 814);
	ASSERT_FALSE(preReq->b->b->b->m_or);
	ASSERT_EQ(preReq->b->b->b->b, nullptr);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[815]->SetMissionState(eMissionState::COMPLETE);
	missionMap[812]->SetMissionState(eMissionState::ACTIVE);
	missionMap[813]->SetMissionState(eMissionState::ACTIVE);
	missionMap[814]->SetMissionState(eMissionState::ACTIVE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	missionMap[815]->SetMissionState(eMissionState::ACTIVE);
	missionMap[812]->SetMissionState(eMissionState::COMPLETE);
	missionMap[813]->SetMissionState(eMissionState::ACTIVE);
	missionMap[814]->SetMissionState(eMissionState::ACTIVE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	missionMap[815]->SetMissionState(eMissionState::ACTIVE);
	missionMap[812]->SetMissionState(eMissionState::ACTIVE);
	missionMap[813]->SetMissionState(eMissionState::COMPLETE);
	missionMap[814]->SetMissionState(eMissionState::ACTIVE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	missionMap[815]->SetMissionState(eMissionState::ACTIVE);
	missionMap[812]->SetMissionState(eMissionState::ACTIVE);
	missionMap[813]->SetMissionState(eMissionState::ACTIVE);
	missionMap[814]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	missionMap[815]->SetMissionState(eMissionState::ACTIVE);
	missionMap[812]->SetMissionState(eMissionState::COMPLETE);
	missionMap[813]->SetMissionState(eMissionState::ACTIVE);
	missionMap[814]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	missionMap[815]->SetMissionState(eMissionState::COMPLETE);
	missionMap[812]->SetMissionState(eMissionState::ACTIVE);
	missionMap[813]->SetMissionState(eMissionState::COMPLETE);
	missionMap[814]->SetMissionState(eMissionState::ACTIVE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	delete preReq;
};

/**
 * Test that the structure of a mission with an expected state is created properly
 */
TEST_F(MissionPrerequisiteTest, MissionStatePrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("236:2");
	missionMap[236] = new Mission();
	ASSERT_EQ(preReq->a, 236);
	ASSERT_EQ(preReq->sub, 2);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[236]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[236]->SetMissionState(eMissionState::ACTIVE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	delete preReq;
};

/**
 * Test that the structure of an or prerequisite for missions is created properly
 */
TEST_F(MissionPrerequisiteTest, AndMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("(2061|1000)&(8091|1500)");
	missionMap[2061] = new Mission();
	missionMap[1000] = new Mission();
	missionMap[8091] = new Mission();
	missionMap[1500] = new Mission();
	ASSERT_EQ(preReq->a, 0);
	ASSERT_FALSE(preReq->m_or);

	ASSERT_EQ(preReq->b->a, 2061);
	ASSERT_TRUE(preReq->b->m_or);

	ASSERT_EQ(preReq->b->b->a, 1000);
	ASSERT_FALSE(preReq->b->b->m_or);

	ASSERT_EQ(preReq->b->b->b->a, 0);
	ASSERT_FALSE(preReq->b->b->b->m_or);

	ASSERT_EQ(preReq->b->b->b->b->a, 8091);
	ASSERT_TRUE(preReq->b->b->b->b->m_or);

	ASSERT_EQ(preReq->b->b->b->b->b->a, 1500);
	ASSERT_FALSE(preReq->b->b->b->b->b->m_or);

	ASSERT_EQ(preReq->b->b->b->b->b->b, nullptr);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[2061]->SetMissionState(eMissionState::COMPLETE);
	missionMap[1000]->SetMissionState(eMissionState::ACTIVE);
	missionMap[8091]->SetMissionState(eMissionState::ACTIVE);
	missionMap[1500]->SetMissionState(eMissionState::ACTIVE);
	ASSERT_FALSE(preReq->Execute(missionMap)); // BUG HERE!!!
	missionMap[2061]->SetMissionState(eMissionState::ACTIVE);
	missionMap[1000]->SetMissionState(eMissionState::COMPLETE);
	missionMap[8091]->SetMissionState(eMissionState::ACTIVE);
	missionMap[1500]->SetMissionState(eMissionState::ACTIVE);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[2061]->SetMissionState(eMissionState::ACTIVE);
	missionMap[1000]->SetMissionState(eMissionState::ACTIVE);
	missionMap[8091]->SetMissionState(eMissionState::COMPLETE);
	missionMap[1500]->SetMissionState(eMissionState::ACTIVE);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[2061]->SetMissionState(eMissionState::ACTIVE);
	missionMap[1000]->SetMissionState(eMissionState::ACTIVE);
	missionMap[8091]->SetMissionState(eMissionState::ACTIVE);
	missionMap[1500]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_FALSE(preReq->Execute(missionMap));
	missionMap[2061]->SetMissionState(eMissionState::COMPLETE);
	missionMap[1000]->SetMissionState(eMissionState::ACTIVE);
	missionMap[8091]->SetMissionState(eMissionState::COMPLETE);
	missionMap[1500]->SetMissionState(eMissionState::ACTIVE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	missionMap[2061]->SetMissionState(eMissionState::COMPLETE);
	missionMap[1000]->SetMissionState(eMissionState::COMPLETE);
	missionMap[8091]->SetMissionState(eMissionState::COMPLETE);
	missionMap[1500]->SetMissionState(eMissionState::COMPLETE);
	ASSERT_TRUE(preReq->Execute(missionMap));
	delete preReq;
};
