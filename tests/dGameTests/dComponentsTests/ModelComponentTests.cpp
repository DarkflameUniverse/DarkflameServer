#include "GameDependencies.h"

#include "BitStream.h"
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "Entity.h"
#include "GameMessages.h"
#include "ModelComponent.h"

#include <gtest/gtest.h>

class ModelComponentTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
	}

	void TearDown() override {
		TearDownDependencies();
	}

	// The bits a model's serialization writes
	static uint32_t SerializedBits(Entity& entity) {
		auto* model = entity.AddComponent<ModelComponent>(-1);
		RakNet::BitStream bitStream;
		model->Serialize(bitStream, true);
		return bitStream.GetNumberOfBitsUsed();
	}
};

// Item block (1 + 64 + 32 + 1), model block (1 + 1 + 32 + 96 + 128)
constexpr uint32_t ModelBlockBits = 98 + 258;
// Behavior info: flag, count, paused, and on the initial update the editing info flag
constexpr uint32_t BehaviorBits = 1 + 32 + 1 + 1;

// The client reads the behaviors only for property and inventory models (ObjectLoader2::LoadModelBehaviorsComponent)
TEST_F(ModelComponentTests, OtherModelsHaveNoBehaviorInfo) {
	Entity entity(1, info);
	EXPECT_EQ(SerializedBits(entity), ModelBlockBits);
}

TEST_F(ModelComponentTests, PropertyModelsHaveBehaviorInfo) {
	EntityInfo propertyInfo = info;
	propertyInfo.settings.Insert<bool>(u"propertyObjectID", true);
	Entity entity(1, propertyInfo);
	EXPECT_EQ(SerializedBits(entity), ModelBlockBits + BehaviorBits);
}

TEST_F(ModelComponentTests, InventoryModelsHaveBehaviorInfo) {
	EntityInfo inventoryInfo = info;
	inventoryInfo.settings.Insert<bool>(u"inInventory", true);
	Entity entity(1, inventoryInfo);
	EXPECT_EQ(SerializedBits(entity), ModelBlockBits + BehaviorBits);
}
