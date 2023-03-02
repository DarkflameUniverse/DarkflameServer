#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "MissionPrerequisites.h"

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
	ASSERT_EQ(preReq->Geta(), 1964);
	delete preReq;
};

/**
 * Test that the structure of a multi mission prerequisite is created correctly.
 */
TEST_F(MissionPrerequisiteTest, MultiMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("1806,1868");
	ASSERT_EQ(preReq->Geta(), 1806);
	ASSERT_EQ(preReq->Getb()->Geta(), 1868);
	delete preReq;
};

/**
 * Test that the structure of an or prerequisite for missions is created properly
 */
TEST_F(MissionPrerequisiteTest, OrMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("2061|891");
	ASSERT_TRUE(preReq->Getm_or());
	ASSERT_EQ(preReq->Geta(), 2061);
	ASSERT_EQ(preReq->Getb()->Geta(), 891);
	delete preReq;
};

/**
 * Test that the structure of an or prerequisite for multiple missions is created properly
 */
TEST_F(MissionPrerequisiteTest, MultiOrMissionPrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("815|812|813|814");
	ASSERT_TRUE(preReq->Getm_or());
	ASSERT_EQ(preReq->Geta(), 815);
	ASSERT_EQ(preReq->Getb()->Geta(), 812);
	ASSERT_TRUE(preReq->Getb()->Getm_or());
	ASSERT_EQ(preReq->Getb()->Geta(), 812);
	ASSERT_EQ(preReq->Getb()->Getb()->Geta(), 813);
	ASSERT_TRUE(preReq->Getb()->Getb()->Getm_or());
	ASSERT_EQ(preReq->Getb()->Getb()->Geta(), 813);
	ASSERT_EQ(preReq->Getb()->Getb()->Getb()->Geta(), 814);
	ASSERT_FALSE(preReq->Getb()->Getb()->Getb()->Getm_or());
	ASSERT_EQ(preReq->Getb()->Getb()->Getb()->Geta(), 814);
	ASSERT_EQ(preReq->Getb()->Getb()->Getb()->Getb(), nullptr);
	delete preReq;
};

/**
 * Test that the structure of a mission with an expected state is created properly
 */
TEST_F(MissionPrerequisiteTest, MissionStatePrerequisiteInitializeTest) {
	auto* preReq = new PrerequisiteExpression("236:2");
	ASSERT_EQ(preReq->Geta(), 236);
	ASSERT_EQ(preReq->Getsub(), 2);
	delete preReq;
};
