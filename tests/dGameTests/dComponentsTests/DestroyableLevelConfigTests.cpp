#include "GameDependencies.h"

#include "DestroyableComponent.h"
#include "Entity.h"

#include <gtest/gtest.h>

// How a level object's config changes its DestroyableComponent, as the client's
// LWODestroyableComponent::LoadConfigData (0x00c44cb0) and LoadDataFromTemplate (0x00c9f900) resolve it.
class DestroyableLevelConfigTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;

	void SetUp() override {
		SetUpDependencies();
		entity = std::make_unique<Entity>(1, info);
	}

	void TearDown() override {
		entity.reset();
		TearDownDependencies();
	}
};

TEST_F(DestroyableLevelConfigTests, NoSetFactionKeepsTheTemplateFactions) {
	EXPECT_FALSE(DestroyableComponent::GetLevelFactions(*entity));
	entity->SetVar<bool>(u"override_faction", true);
	EXPECT_FALSE(DestroyableComponent::GetLevelFactions(*entity));
}

TEST_F(DestroyableLevelConfigTests, SetFactionWithoutOverrideFactionIsUsed) {
	entity->SetVar<std::string>(u"set_faction", "6 ");
	EXPECT_EQ(DestroyableComponent::GetLevelFactions(*entity), std::vector<int32_t>{ 6 });
}

TEST_F(DestroyableLevelConfigTests, SetFactionWithOverrideFactionTrueIsUsed) {
	entity->SetVar<std::string>(u"set_faction", "4; 6");
	entity->SetVar<bool>(u"override_faction", true);
	EXPECT_EQ(DestroyableComponent::GetLevelFactions(*entity), (std::vector<int32_t>{ 4, 6 }));
}

TEST_F(DestroyableLevelConfigTests, OverrideFactionFalseIgnoresSetFaction) {
	entity->SetVar<std::string>(u"set_faction", "-1");
	entity->SetVar<bool>(u"override_faction", false);
	EXPECT_FALSE(DestroyableComponent::GetLevelFactions(*entity));
}
