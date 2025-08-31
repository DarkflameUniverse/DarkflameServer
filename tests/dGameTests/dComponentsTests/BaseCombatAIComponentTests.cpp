#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "BaseCombatAIComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class BaseCombatAITest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	BaseCombatAIComponent* combatAIComponent;
	CBITSTREAM
	uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		combatAIComponent = baseEntity->AddComponent<BaseCombatAIComponent>(1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test serialization of BaseCombatAIComponent in initial update with default spawn state
 */
TEST_F(BaseCombatAITest, BaseCombatAIComponentSerializeInitialUpdateTest) {
	bitStream.Reset();
	
	// Component should be dirty by default and in spawn state
	combatAIComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, true); // Should always be true for initial update
	
	uint32_t state;
	bitStream.Read(state);
	EXPECT_EQ(state, static_cast<uint32_t>(AiState::spawn)); // Default state is spawn
	
	LWOOBJID target;
	bitStream.Read(target);
	EXPECT_EQ(target, LWOOBJID_EMPTY); // No target by default
}

/**
 * Test serialization of BaseCombatAIComponent in regular update with clean state
 */
TEST_F(BaseCombatAITest, BaseCombatAIComponentSerializeRegularUpdateTest) {
	bitStream.Reset();
	
	// First serialize to clear dirty flag
	combatAIComponent->Serialize(bitStream, false); // This clears the dirty flag
	bitStream.Reset();
	
	// Now serialize again - should not be dirty
	combatAIComponent->Serialize(bitStream, false);
	
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, false); // Should not be dirty after previous serialization
}

/**
 * Test serialization when target changes during regular updates
 */
TEST_F(BaseCombatAITest, BaseCombatAIComponentSerializeTargetChangeTest) {
	bitStream.Reset();
	
	// Initial state is spawn, serialize once to clear dirty flag
	combatAIComponent->Serialize(bitStream, false);
	bitStream.Reset();
	
	// Change target - this should set dirty flag
	LWOOBJID testTarget = 12345;
	combatAIComponent->SetTarget(testTarget);
	combatAIComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, true); // Should be dirty due to target change
	
	uint32_t state;
	bitStream.Read(state);
	EXPECT_EQ(state, static_cast<uint32_t>(AiState::spawn)); // Still in spawn state
	
	LWOOBJID target;
	bitStream.Read(target);
	EXPECT_EQ(target, testTarget);
}

/**
 * Test serialization with target management and getters/setters
 */
TEST_F(BaseCombatAITest, BaseCombatAIComponentSerializeWithTargetTest) {
	bitStream.Reset();
	
	// Set a target and change state manually
	LWOOBJID testTarget = 12345;
	combatAIComponent->SetTarget(testTarget);
	combatAIComponent->SetState(AiState::tether);
	
	combatAIComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, true); // Should be dirty due to target change
	
	uint32_t state;
	bitStream.Read(state);
	EXPECT_EQ(state, static_cast<uint32_t>(AiState::tether));
	
	LWOOBJID target;
	bitStream.Read(target);
	EXPECT_EQ(target, testTarget);
	
	// Verify component state
	EXPECT_EQ(combatAIComponent->GetTarget(), testTarget);
	EXPECT_EQ(combatAIComponent->GetState(), AiState::tether);
}