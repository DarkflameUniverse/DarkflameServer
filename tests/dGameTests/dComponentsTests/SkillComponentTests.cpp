#include "GameDependencies.h"

#include "Behavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
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

