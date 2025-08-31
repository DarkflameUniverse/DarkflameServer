#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "ScriptComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

class ScriptComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	ScriptComponent* scriptComponent;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		scriptComponent = baseEntity->AddComponent<ScriptComponent>();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(ScriptComponentTest, ScriptComponentSerializeInitialEmptyTest) {
	scriptComponent->Serialize(bitStream, true);
	
	// Should write network settings flag (false for empty)
	bool hasNetworkSettings;
	bitStream.Read(hasNetworkSettings);
	ASSERT_FALSE(hasNetworkSettings);
	
	// That should be all for empty network settings
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 1);
}

TEST_F(ScriptComponentTest, ScriptComponentSerializeUpdateTest) {
	// Non-initial updates should not write anything for ScriptComponent
	scriptComponent->Serialize(bitStream, false);
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}



TEST_F(ScriptComponentTest, ScriptComponentSerializedFlagTest) {
	// Test the serialized flag functionality
	ASSERT_FALSE(scriptComponent->GetSerialized());
	
	scriptComponent->SetSerialized(true);
	ASSERT_TRUE(scriptComponent->GetSerialized());
	
	scriptComponent->SetSerialized(false);
	ASSERT_FALSE(scriptComponent->GetSerialized());
}