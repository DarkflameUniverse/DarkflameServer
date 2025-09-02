#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "LUPExhibitComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class LUPExhibitComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	LUPExhibitComponent* lupExhibitComponent;
	CBITSTREAM
	uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		lupExhibitComponent = baseEntity->AddComponent<LUPExhibitComponent>();
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test LUPExhibitComponent initial serialization
 */
TEST_F(LUPExhibitComponentTest, SerializeInitialUpdateTest) {
	bitStream.Reset();
	
	// Component should be dirty by default
	lupExhibitComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, true); // Should be dirty by default
	
	LOT exhibitLOT;
	bitStream.Read(exhibitLOT);
	EXPECT_EQ(exhibitLOT, 11121); // First exhibit in the array
}

/**
 * Test LUPExhibitComponent regular update when not dirty
 */
TEST_F(LUPExhibitComponentTest, SerializeNotDirtyTest) {
	bitStream.Reset();
	
	// First serialize to clear dirty flag
	lupExhibitComponent->Serialize(bitStream, false); // This clears the dirty flag
	bitStream.Reset();
	
	// Now serialize again - should not be dirty
	lupExhibitComponent->Serialize(bitStream, false);
	
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, false); // Should not be dirty after previous serialization
}

/**
 * Test LUPExhibitComponent cycling through exhibits
 */
TEST_F(LUPExhibitComponentTest, NextExhibitTest) {
	bitStream.Reset();
	
	// Get first exhibit
	lupExhibitComponent->Serialize(bitStream, true);
	
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, true);
	
	LOT firstExhibit;
	bitStream.Read(firstExhibit);
	EXPECT_EQ(firstExhibit, 11121); // First exhibit
	
	bitStream.Reset();
	
	// Move to next exhibit
	lupExhibitComponent->NextLUPExhibit();
	lupExhibitComponent->Serialize(bitStream, false);
	
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, true); // Should be dirty after NextLUPExhibit
	
	LOT secondExhibit;
	bitStream.Read(secondExhibit);
	EXPECT_EQ(secondExhibit, 11295); // Second exhibit
}

/**
 * Test LUPExhibitComponent cycling through all exhibits and wrapping around
 */
TEST_F(LUPExhibitComponentTest, CycleAllExhibitsTest) {
	bitStream.Reset();
	
	// Expected exhibit sequence: 11121, 11295, 11423, 11979, then back to 11121
	std::array<LOT, 5> expectedLOTs = { 11121, 11295, 11423, 11979, 11121 };
	
	for (size_t i = 0; i < expectedLOTs.size(); ++i) {
		if (i > 0) {
			lupExhibitComponent->NextLUPExhibit();
		}
		
		bitStream.Reset();
		lupExhibitComponent->Serialize(bitStream, false);
		
		bool isDirty;
		bitStream.Read(isDirty);
		EXPECT_EQ(isDirty, true);
		
		LOT exhibitLOT;
		bitStream.Read(exhibitLOT);
		EXPECT_EQ(exhibitLOT, expectedLOTs[i]) << "Exhibit " << i << " should be " << expectedLOTs[i];
	}
}
