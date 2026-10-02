#include "GameDependencies.h"

#include "Behavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "CDClientManager.h"
#include "CDSkillBehaviorTable.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "SkillComponent.h"

#include <gtest/gtest.h>

namespace {
	// A behavior that waits for one client sync (handle 7) and counts the syncs it gets
	class WaitsForSync final : public Behavior {
	public:
		static constexpr uint32_t SYNC_HANDLE = 7;
		int syncs = 0;

		explicit WaitsForSync(const uint32_t behaviorId) : Behavior(behaviorId) {}

		void Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override {
			context->RegisterSyncBehavior(SYNC_HANDLE, this, branch, 10.0f);
		}

		void Sync(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) override { ++syncs; }
	};
}

class SkillComponentTests : public GameDependenciesTest {
protected:
	static constexpr uint32_t BEHAVIOR_ID = 990001;
	WaitsForSync* behavior = nullptr;

	void SetUp() override {
		SetUpDependencies();
		behavior = new WaitsForSync(BEHAVIOR_ID);
	}

	void TearDown() override {
		Behavior::Cache.erase(BEHAVIOR_ID);
		delete behavior;
		TearDownDependencies();
	}
};

// LWOSkillComponent::msgStartSkill (0x00d45cb0): a start whose skill handle still waits on syncs is logged and dropped;
// once its behaviors are synced the handle can start a skill again.
TEST_F(SkillComponentTests, StartWithAPendingSkillHandleIsDropped) {
	Entity caster(30, info);
	auto* const skills = caster.AddComponent<SkillComponent>(-1);
	RakNet::BitStream empty;

	EXPECT_TRUE(skills->CastPlayerSkill(BEHAVIOR_ID, 5, empty, LWOOBJID_EMPTY));
	EXPECT_TRUE(skills->IsSkillPending(5));
	EXPECT_FALSE(skills->CastPlayerSkill(BEHAVIOR_ID, 5, empty, LWOOBJID_EMPTY));
	EXPECT_TRUE(skills->CastPlayerSkill(BEHAVIOR_ID, 6, empty, LWOOBJID_EMPTY)); // another handle is free

	skills->SyncPlayerSkill(5, WaitsForSync::SYNC_HANDLE, empty);
	EXPECT_EQ(behavior->syncs, 1); // the dropped start registered no second sync
	EXPECT_FALSE(skills->IsSkillPending(5));
	EXPECT_TRUE(skills->CastPlayerSkill(BEHAVIOR_ID, 5, empty, LWOOBJID_EMPTY));
}

// LWOSkillComponent::MsgSyncSkill (0x00d454b0): a sync for a dead caster ends that behavior without running it.
TEST_F(SkillComponentTests, DeadCastersSyncIsDroppedUnrun) {
	Entity caster(31, info);
	auto* const destroyable = caster.AddComponent<DestroyableComponent>(-1);
	auto* const skills = caster.AddComponent<SkillComponent>(-1);
	RakNet::BitStream empty;

	ASSERT_TRUE(skills->CastPlayerSkill(BEHAVIOR_ID, 1, empty, LWOOBJID_EMPTY));
	destroyable->SetIsDead(true);
	skills->SyncPlayerSkill(1, WaitsForSync::SYNC_HANDLE, empty);
	EXPECT_EQ(behavior->syncs, 0);
	EXPECT_FALSE(skills->IsSkillPending(1));

	destroyable->SetIsDead(false);
	ASSERT_TRUE(skills->CastPlayerSkill(BEHAVIOR_ID, 2, empty, LWOOBJID_EMPTY));
	skills->SyncPlayerSkill(2, WaitsForSync::SYNC_HANDLE, empty);
	EXPECT_EQ(behavior->syncs, 1);
}

// LWOSkillComponent::msgStartSkill (0x00d45cb0): cast types 1 and 2 start no cooldown, and a cooldown group already
// running keeps its time left.
TEST_F(SkillComponentTests, CooldownsFollowTheClient) {
	CDSkillBehavior shortSkill{};
	shortSkill.skillID = 394;
	shortSkill.cooldowngroup = 17;
	shortSkill.cooldown = 5.0f;
	CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>()[394] = shortSkill;
	CDSkillBehavior longSkill = shortSkill;
	longSkill.skillID = 395;
	longSkill.cooldown = 30.0f;
	CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>()[395] = longSkill;

	auto playerInfo = info;
	playerInfo.lot = 1;
	Entity player(32, playerInfo);
	Character character(1, nullptr);
	SystemAddress address;
	address.binaryAddress = 0x0100007f;
	address.port = 2003;
	player.AddComponent<CharacterComponent>(-1, &character, address);
	ASSERT_TRUE(player.IsPlayer());
	auto* const skills = player.AddComponent<SkillComponent>(-1);
	RakNet::BitStream empty;

	ASSERT_TRUE(skills->CastPlayerSkill(0, 1, empty, LWOOBJID_EMPTY, 394, 1));
	ASSERT_TRUE(skills->CastPlayerSkill(0, 2, empty, LWOOBJID_EMPTY, 394, 2));
	EXPECT_TRUE(skills->GetCooldownGroups().empty());

	ASSERT_TRUE(skills->CastPlayerSkill(0, 3, empty, LWOOBJID_EMPTY, 394, 0));
	skills->Run(1.0f);
	ASSERT_TRUE(skills->CastPlayerSkill(0, 4, empty, LWOOBJID_EMPTY, 395, 4));
	ASSERT_EQ(skills->GetCooldownGroups().size(), 1u);
	EXPECT_FLOAT_EQ(skills->GetCooldownGroups().at(17), 4.0f);

	CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>().clear();
}
