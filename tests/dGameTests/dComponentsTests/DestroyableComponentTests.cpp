#include <gtest/gtest.h>
#include "Entity.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "DestroyableComponent.h"
#include "../GameDependencies.h"

TEST_F(GameDependenciesTest, dDestroyableComponentBasicTest) {
	auto entity = new Entity(14, info);
	auto destroyableComponent = new DestroyableComponent(entity);
	destroyableComponent->SetMaxArmor(1.0f);
	ASSERT_EQ(destroyableComponent->GetMaxArmor(), 1.0f);
}
