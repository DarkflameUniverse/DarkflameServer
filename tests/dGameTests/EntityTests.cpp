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

	void RunWhitelistTest(const int32_t componentIndex, std::vector<eReplicaComponentType>& whitelist) {
		entity->SetVar<int32_t>(u"componentWhitelist", componentIndex);
		entity->ApplyComponentWhitelist(whitelist);
		const auto whitelists = Entity::GetComponentWhitelists();
		for (const auto& component : whitelists.at(componentIndex)) {
			EXPECT_FALSE(std::find(whitelist.begin(), whitelist.end(), component) != whitelist.end());
		}
	}
};

TEST_F(EntityTests, WhitelistTest) {
	const auto whitelists = Entity::GetComponentWhitelists();
	std::vector<eReplicaComponentType> whitelist = {
		eReplicaComponentType::RACING_STATS,
		eReplicaComponentType::SIMPLE_PHYSICS,
		eReplicaComponentType::CONTROLLABLE_PHYSICS,
		eReplicaComponentType::RENDER,
		eReplicaComponentType::DESTROYABLE,
		eReplicaComponentType::RACING_CONTROL
	};
	RunWhitelistTest(0, whitelist);
	RunWhitelistTest(1, whitelist);
	RunWhitelistTest(2, whitelist);
	RunWhitelistTest(3, whitelist);
	RunWhitelistTest(4, whitelist);
}
