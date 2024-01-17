#include "GameDependencies.h"
#include <gtest/gtest.h>
#include <functional>

#include "dCommonVars.h"
#include "Entity.h"
#include "EntityInfo.h"
#include "EntityTimer.h"
#include "EntityCallbackTimer.h"
#include "ScriptComponent.h"

class EntityMock : public Entity {
public:
	EntityMock(const LWOOBJID& objectID, EntityInfo info, Entity* parentEntity = nullptr) : Entity(objectID, info, parentEntity) {}
	std::vector<EntityTimer>& GetTimers() { return m_Timers; }
	std::vector<EntityCallbackTimer>& GetCallbackTimers() { return m_CallbackTimers; }
};

class EntityTest : public GameDependenciesTest {
protected:
	EntityMock* entity;

	void SetUp() override {
		SetUpDependencies();
		entity = new EntityMock(15, GameDependenciesTest::info);
		entity->AddComponent<ScriptComponent>("scripts\\02_server\\DLU\\TimerTestScript.lua", false, false);
	}

	void TearDown() override {
		delete entity;
		TearDownDependencies();
	}
};

TEST_F(EntityTest, EntityTimerTests) {
	entity->AddTimer("addTwoTimers", 0.5f);
	ASSERT_TRUE(entity->HasTimer("addTwoTimers"));
	ASSERT_TRUE(entity->GetTimers().size() == 1);
	entity->AddCallbackTimer(1.0f, [](){});
	ASSERT_TRUE(entity->GetCallbackTimers().size() == 1);
	entity->Update(1.1f);
	ASSERT_EQ(entity->GetTimers().size(), 2);
	ASSERT_TRUE(entity->GetCallbackTimers().empty());
}

// The attached script has a listener for the timer "testTimer" that adds two timers when it's done, both of time 99.0f
// then we check that all timers have a time of 99.0
TEST_F(EntityTest, EntityTimerEdgeCaseTests) {
	entity->AddTimer("addTwoTimers", 0.5f); // Will be removed and should swap places with addedBefore3.
	entity->AddTimer("addedBefore1", 100.0f);
	entity->AddTimer("addedBefore2", 1.0f); // Will be removed and will not swap places with anything.
	entity->AddTimer("addedBefore3", 12.0f);
	entity->Update(1.0f);
	entity->AddTimer("addedAfter1", 15.0f);
	entity->AddTimer("addedAfter2", 16.0f);
	entity->AddTimer("addedAfter3", 17.0f);

	ASSERT_EQ(entity->GetTimers().size(), 9);
	auto TimerTestFunction = [this](int32_t index, std::string timerName, float time) {
		ASSERT_EQ(entity->GetTimers().at(index).GetName(), timerName);
		ASSERT_EQ(entity->GetTimers().at(index).GetTime(), time);
	};

	// First two are the timers remaining from the update that were added before the call.
	// Observe that addedBefore3 has been moved to the front and properly decremented.
	TimerTestFunction(0, "addedBefore3", 11.0f);
	TimerTestFunction(1, "addedBefore1", 99.0f);

	// Second two timers are the ones added by the script, just here to make sure they are in exactly position 2 and 3 and were not moved forward in the vector.
	TimerTestFunction(2, "expiredAddTwoTimers1", 10.0f);
	TimerTestFunction(3, "expiredAddTwoTimers2", 11.0f);

	// The next two are added on the expiration of addedBefore2, and are only here to make sure that timer expired properly since it would be at the end of the updated timers.
	TimerTestFunction(4, "expiredAddedBefore1", 12.0f);
	TimerTestFunction(5, "expiredAddedBefore2", 13.0f);

	// Last 3 are added afterwards and should be at the end of the vector.
	TimerTestFunction(6, "addedAfter1", 15.0f);
	TimerTestFunction(7, "addedAfter2", 16.0f);
	TimerTestFunction(8, "addedAfter3", 17.0f);
}

TEST_F(EntityTest, EntityCallbackTimerTest) {
	auto emptyLambda = [](){};

	auto callbackToAddCallbackTimer = [this, emptyLambda]() {
		entity->AddCallbackTimer(0.5f, emptyLambda);
	};

	auto callbackToAddCallbackTimer2 = [this, emptyLambda]() {
		entity->AddCallbackTimer(4.5f, emptyLambda);
	};

	auto callbackTimerToAddTwoTimers = [this, callbackToAddCallbackTimer]() {
		entity->AddCallbackTimer(0.5f, callbackToAddCallbackTimer);
		entity->AddCallbackTimer(0.5f, callbackToAddCallbackTimer);
	};

	auto callbackTimerToAddTwoTimers2 = [this, callbackToAddCallbackTimer]() {
		entity->AddCallbackTimer(3.5f, callbackToAddCallbackTimer);
		entity->AddCallbackTimer(3.5f, callbackToAddCallbackTimer);
	};

	entity->AddCallbackTimer(0.5f, emptyLambda);
	entity->AddCallbackTimer(4.5f, callbackToAddCallbackTimer);
	entity->AddCallbackTimer(0.5f, callbackToAddCallbackTimer2);
	entity->AddCallbackTimer(1.5f, callbackTimerToAddTwoTimers);
	entity->AddCallbackTimer(2.5f, callbackTimerToAddTwoTimers2);
	entity->Update(1.0f);

	ASSERT_EQ(entity->GetCallbackTimers().size(), 4);
	
	auto TestingFunction = [this](int32_t index, float time, std::function<void()> callback) {
		ASSERT_TRUE(index < entity->GetCallbackTimers().size());
		ASSERT_EQ(entity->GetCallbackTimers().at(index).GetTime(), time);
		ASSERT_EQ(entity->GetCallbackTimers().at(index).GetCallback().target_type(), callback.target_type());
	};

	TestingFunction(0, 1.5f, callbackTimerToAddTwoTimers2);
	TestingFunction(1, 3.5f, callbackToAddCallbackTimer);
	TestingFunction(2, 0.5f, callbackTimerToAddTwoTimers);
	TestingFunction(3, 4.5f, emptyLambda);
}
