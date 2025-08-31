#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "ModelComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "PetComponent.h"

class ModelTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	ModelComponent* modelComponent;
	CBITSTREAM
		uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		modelComponent = baseEntity->AddComponent<ModelComponent>();
		// Initialize some values to be not default
		modelComponent->SetPosition(NiPoint3(10.0f, 20.0f, 30.0f));
		modelComponent->SetRotation(NiQuaternion(4.0f, 1.0f, 2.0f, 3.0f)); // For x=1, y=2, z=3, w=4
		modelComponent->AddInteract(); // Make it pickable
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test serialization of a ModelComponent for a non-pet entity
 */
TEST_F(ModelTest, ModelComponentSerializeNonPetTest) {
	bitStream.Reset();
	
	// Now we test a serialization for correctness.
	modelComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	
	// ItemComponent serialization (since this is not a pet)
	bool hasItemComponent;
	bitStream.Read(hasItemComponent);
	EXPECT_EQ(hasItemComponent, true);
	
	LWOOBJID userModelID;
	bitStream.Read(userModelID);
	EXPECT_EQ(userModelID, baseEntity->GetObjectID()); // Should use entity ID when no user model ID
	
	int extraInfo;
	bitStream.Read(extraInfo);
	EXPECT_EQ(extraInfo, 0);
	
	bool hasExtraItemData;
	bitStream.Read(hasExtraItemData);
	EXPECT_EQ(hasExtraItemData, false);
	
	// ModelComponent serialization
	bool hasModelInfo;
	bitStream.Read(hasModelInfo);
	EXPECT_EQ(hasModelInfo, true);
	
	bool isPickable;
	bitStream.Read(isPickable);
	EXPECT_EQ(isPickable, true); // We added an interact
	
	uint32_t physicsType;
	bitStream.Read(physicsType);
	EXPECT_EQ(physicsType, 2);
	
	NiPoint3 originalPosition;
	bitStream.Read(originalPosition.x);
	bitStream.Read(originalPosition.y);
	bitStream.Read(originalPosition.z);
	EXPECT_EQ(originalPosition, NiPoint3(10.0f, 20.0f, 30.0f));
	
	NiQuaternion originalRotation;
	bitStream.Read(originalRotation.x);
	bitStream.Read(originalRotation.y);
	bitStream.Read(originalRotation.z);
	bitStream.Read(originalRotation.w);
	EXPECT_EQ(originalRotation, NiQuaternion(4.0f, 1.0f, 2.0f, 3.0f)); // x=1, y=2, z=3, w=4
	
	bool hasBehaviorInfo;
	bitStream.Read(hasBehaviorInfo);
	EXPECT_EQ(hasBehaviorInfo, true);
	
	uint32_t numBehaviors;
	bitStream.Read(numBehaviors);
	EXPECT_EQ(numBehaviors, 0); // No behaviors added in test
	
	bool isPaused;
	bitStream.Read(isPaused);
	EXPECT_EQ(isPaused, false);
}

/**
 * Test serialization of a ModelComponent for a pet entity
 */
TEST_F(ModelTest, ModelComponentSerializePetTest) {
	bitStream.Reset();
	
	// Add a PetComponent to make this entity a pet
	baseEntity->AddComponent<PetComponent>(1);
	
	// Now we test a serialization for correctness.
	modelComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	// Should NOT have ItemComponent serialization for pets
	
	// ModelComponent serialization (should start immediately)
	bool hasModelInfo;
	bitStream.Read(hasModelInfo);
	EXPECT_EQ(hasModelInfo, true);
	
	bool isPickable;
	bitStream.Read(isPickable);
	EXPECT_EQ(isPickable, true); // We added an interact
	
	uint32_t physicsType;
	bitStream.Read(physicsType);
	EXPECT_EQ(physicsType, 2);
	
	NiPoint3 originalPosition;
	bitStream.Read(originalPosition.x);
	bitStream.Read(originalPosition.y);
	bitStream.Read(originalPosition.z);
	EXPECT_EQ(originalPosition, NiPoint3(10.0f, 20.0f, 30.0f));
	
	NiQuaternion originalRotation;
	bitStream.Read(originalRotation.x);
	bitStream.Read(originalRotation.y);
	bitStream.Read(originalRotation.z);
	bitStream.Read(originalRotation.w);
	EXPECT_EQ(originalRotation, NiQuaternion(4.0f, 1.0f, 2.0f, 3.0f)); // x=1, y=2, z=3, w=4
	
	bool hasBehaviorInfo;
	bitStream.Read(hasBehaviorInfo);
	EXPECT_EQ(hasBehaviorInfo, true);
	
	uint32_t numBehaviors;
	bitStream.Read(numBehaviors);
	EXPECT_EQ(numBehaviors, 0); // No behaviors added in test
	
	bool isPaused;
	bitStream.Read(isPaused);
	EXPECT_EQ(isPaused, false);
}

/**
 * Test serialization of a ModelComponent during initial update
 */
TEST_F(ModelTest, ModelComponentSerializeInitialUpdateTest) {
	bitStream.Reset();
	
	// Now we test a serialization for correctness with initial update.
	modelComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	
	// ItemComponent serialization (since this is not a pet)
	bool hasItemComponent;
	bitStream.Read(hasItemComponent);
	EXPECT_EQ(hasItemComponent, true);
	
	LWOOBJID userModelID;
	bitStream.Read(userModelID);
	EXPECT_EQ(userModelID, baseEntity->GetObjectID());
	
	int extraInfo;
	bitStream.Read(extraInfo);
	EXPECT_EQ(extraInfo, 0);
	
	bool hasExtraItemData;
	bitStream.Read(hasExtraItemData);
	EXPECT_EQ(hasExtraItemData, false);
	
	// ModelComponent serialization
	bool hasModelInfo;
	bitStream.Read(hasModelInfo);
	EXPECT_EQ(hasModelInfo, true);
	
	bool isPickable;
	bitStream.Read(isPickable);
	EXPECT_EQ(isPickable, true);
	
	uint32_t physicsType;
	bitStream.Read(physicsType);
	EXPECT_EQ(physicsType, 2);
	
	NiPoint3 originalPosition;
	bitStream.Read(originalPosition.x);
	bitStream.Read(originalPosition.y);
	bitStream.Read(originalPosition.z);
	EXPECT_EQ(originalPosition, NiPoint3(10.0f, 20.0f, 30.0f));
	
	NiQuaternion originalRotation;
	bitStream.Read(originalRotation.x);
	bitStream.Read(originalRotation.y);
	bitStream.Read(originalRotation.z);
	bitStream.Read(originalRotation.w);
	EXPECT_EQ(originalRotation, NiQuaternion(4.0f, 1.0f, 2.0f, 3.0f)); // x=1, y=2, z=3, w=4
	
	bool hasBehaviorInfo;
	bitStream.Read(hasBehaviorInfo);
	EXPECT_EQ(hasBehaviorInfo, true);
	
	uint32_t numBehaviors;
	bitStream.Read(numBehaviors);
	EXPECT_EQ(numBehaviors, 0);
	
	bool isPaused;
	bitStream.Read(isPaused);
	EXPECT_EQ(isPaused, false);
	
	// During initial update, should write an additional false for model editing info
	bool hasModelEditingInfo;
	bitStream.Read(hasModelEditingInfo);
	EXPECT_EQ(hasModelEditingInfo, false);
}

/**
 * Test ModelComponent getters and setters
 */
TEST_F(ModelTest, ModelComponentGettersSettersTest) {
	// Test position
	NiPoint3 testPosition(100.0f, 200.0f, 300.0f);
	modelComponent->SetPosition(testPosition);
	EXPECT_EQ(modelComponent->GetOriginalPosition(), testPosition);
	
	// Test rotation
	NiQuaternion testRotation(5.0f, 6.0f, 7.0f, 8.0f);
	modelComponent->SetRotation(testRotation);
	EXPECT_EQ(modelComponent->GetOriginalRotation(), testRotation);
	
	// Test speed
	modelComponent->SetSpeed(5.5f);
	// Note: GetSpeed() method doesn't exist in the header, but we can verify the setter works
	
	// Test interact
	modelComponent->RemoveInteract(); // Remove the one we added in SetUp
	// Test that isPickable becomes false when no interactions
	bitStream.Reset();
	modelComponent->Serialize(bitStream, false);
	
	// Skip itemcomponent data
	bool hasItemComponent;
	bitStream.Read(hasItemComponent);
	LWOOBJID userModelID;
	bitStream.Read(userModelID);
	int extraInfo;
	bitStream.Read(extraInfo);
	bool hasExtraItemData;
	bitStream.Read(hasExtraItemData);
	
	// Check model component
	bool hasModelInfo;
	bitStream.Read(hasModelInfo);
	bool isPickable;
	bitStream.Read(isPickable);
	EXPECT_EQ(isPickable, false); // Should be false now
}