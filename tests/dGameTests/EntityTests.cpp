#include <gtest/gtest.h>


#include <memory>

#include "Entity.h"
#include "GameDependencies.h"
#include "dCommonVars.h"
#include "eReplicaComponentType.h"

class EntityTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;

	virtual void SetUp() {
		entity = std::move(std::make_unique<Entity>(LWOOBJID_EMPTY, EntityInfo()));
		this->SetUpDependencies();
	}

	virtual void TearDown() {
		this->TearDownDependencies();
	}

	void RunWhitelistTest(const int32_t whitelistIndex, std::vector<eReplicaComponentType> componentList) {
		Game::logger->Log("EntityTests", "whitelist test %i", whitelistIndex);
		entity->SetVar<int32_t>(u"componentWhitelist", whitelistIndex);
		entity->ApplyComponentWhitelist(componentList);
		const auto whitelist = Entity::GetComponentWhitelists().at(whitelistIndex);
		std::for_each(whitelist.begin(), whitelist.end(), [&componentList](const eReplicaComponentType& keptComponent) {
			EXPECT_EQ(std::count(componentList.begin(), componentList.end(), keptComponent), 2);
		});
	}
};

TEST_F(EntityTests, WhitelistTest) {
	const auto whitelists = Entity::GetComponentWhitelists();
	std::vector<eReplicaComponentType> components = {
		eReplicaComponentType::CONTROLLABLE_PHYSICS,
		eReplicaComponentType::SIMPLE_PHYSICS,
		eReplicaComponentType::MODEL_BEHAVIOR,
		eReplicaComponentType::SPAWN,
		eReplicaComponentType::RENDER,
		eReplicaComponentType::ITEM,
		eReplicaComponentType::BLUEPRINT,
		eReplicaComponentType::PET,
		eReplicaComponentType::SKILL,
		eReplicaComponentType::DESTROYABLE,
		eReplicaComponentType::CONTROLLABLE_PHYSICS,
		eReplicaComponentType::SIMPLE_PHYSICS,
		eReplicaComponentType::MODEL_BEHAVIOR,
		eReplicaComponentType::SPAWN,
		eReplicaComponentType::RENDER,
		eReplicaComponentType::ITEM,
		eReplicaComponentType::BLUEPRINT,
		eReplicaComponentType::PET,
		eReplicaComponentType::SKILL,
		eReplicaComponentType::DESTROYABLE,
	};
	RunWhitelistTest(0, components);
	RunWhitelistTest(1, components);
	RunWhitelistTest(2, components);
	RunWhitelistTest(3, components);
	RunWhitelistTest(4, components);
}
