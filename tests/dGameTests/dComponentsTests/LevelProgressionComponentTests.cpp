#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "LevelProgressionComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class LevelProgressionComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	LevelProgressionComponent* levelProgressionComponent;
	CBITSTREAM
	uint32_t flags = 0;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		levelProgressionComponent = baseEntity->AddComponent<LevelProgressionComponent>();
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

// Test initial serialization
TEST_F(LevelProgressionComponentTest, InitialSerialization) {
	levelProgressionComponent->Serialize(bitStream, true);
	
	// Should write dirty flag and level
	bool dirtyFlag;
	uint32_t level;
	
	bitStream.Read(dirtyFlag);
	EXPECT_TRUE(dirtyFlag); // Should be true for initial update
	
	bitStream.Read(level);
	EXPECT_EQ(level, 1); // Default level should be 1
	
	bitStream.Reset();
}

// Test regular update without level change
TEST_F(LevelProgressionComponentTest, RegularUpdateWithoutChange) {
	// First do initial serialization to clear dirty flag
	levelProgressionComponent->Serialize(bitStream, true);
	bitStream.Reset();
	
	// Now do regular update
	levelProgressionComponent->Serialize(bitStream, false);
	
	bool dirtyFlag;
	bitStream.Read(dirtyFlag);
	EXPECT_FALSE(dirtyFlag); // Should be false since level didn't change
	
	bitStream.Reset();
}

// Test serialization after level change
TEST_F(LevelProgressionComponentTest, SerializationAfterLevelChange) {
	// Set a different level
	levelProgressionComponent->SetLevel(5);
	
	levelProgressionComponent->Serialize(bitStream, false);
	
	bool dirtyFlag;
	uint32_t level;
	
	bitStream.Read(dirtyFlag);
	EXPECT_TRUE(dirtyFlag); // Should be true since level changed
	
	bitStream.Read(level);
	EXPECT_EQ(level, 5);
	
	bitStream.Reset();
}

// Test that dirty flag gets cleared after serialization
TEST_F(LevelProgressionComponentTest, DirtyFlagClearedAfterSerialization) {
	// Change level to make it dirty
	levelProgressionComponent->SetLevel(3);
	
	// First serialization
	levelProgressionComponent->Serialize(bitStream, false);
	bitStream.Reset();
	
	// Second serialization should not be dirty
	levelProgressionComponent->Serialize(bitStream, false);
	
	bool dirtyFlag;
	bitStream.Read(dirtyFlag);
	EXPECT_FALSE(dirtyFlag); // Should be false since dirty flag was cleared
	
	bitStream.Reset();
}