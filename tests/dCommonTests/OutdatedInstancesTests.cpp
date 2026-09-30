#include <gtest/gtest.h>

#include "OutdatedInstances.h"

using namespace OutdatedInstances;
using InstanceMigration::InstanceView;

namespace {
	InstanceView View(uint32_t zone, uint32_t instance, int32_t players, uint32_t clone = 0) {
		InstanceView view;
		view.zoneId = zone;
		view.instanceId = instance;
		view.cloneId = clone;
		view.players = players;
		view.softCap = 8;
		view.hardCap = 12;
		view.ready = true;
		return view;
	}

	const Clock::time_point START = Clock::time_point{} + std::chrono::hours(1);
}

TEST(OutdatedInstancesTest, NoticeOnceThenEveryTenMinutes) {
	auto property = View(1150, 1, 2, 42);
	EXPECT_FALSE(NoticeDue(property, std::nullopt, START)) << "not outdated";
	property.outdated = true;
	EXPECT_TRUE(NoticeDue(property, std::nullopt, START)) << "at once";
	EXPECT_FALSE(NoticeDue(property, START, START + std::chrono::minutes(1)));
	EXPECT_FALSE(NoticeDue(property, START, START + std::chrono::minutes(9) + std::chrono::seconds(59)));
	EXPECT_TRUE(NoticeDue(property, START, START + std::chrono::minutes(10)));
	EXPECT_TRUE(NoticeDue(property, START, START + std::chrono::minutes(25)));
	EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(NOTICE_INTERVAL).count(), 600);
}

TEST(OutdatedInstancesTest, NoticeOnlyForPropertiesWithPlayers) {
	auto world = View(1100, 1, 3);
	world.outdated = true;
	EXPECT_FALSE(NoticeDue(world, std::nullopt, START)) << "other worlds' players are moved instead";
	auto empty = View(1150, 2, 0, 42);
	empty.outdated = true;
	EXPECT_FALSE(NoticeDue(empty, std::nullopt, START));
	auto stopping = View(1150, 3, 1, 42);
	stopping.outdated = true;
	stopping.shuttingDown = true;
	EXPECT_FALSE(NoticeDue(stopping, std::nullopt, START));
	auto starting = View(1150, 4, 1, 42);
	starting.outdated = true;
	starting.ready = false;
	EXPECT_FALSE(NoticeDue(starting, std::nullopt, START));
}

TEST(OutdatedInstancesTest, StopsOnceEmpty) {
	auto property = View(1150, 1, 2, 42);
	property.outdated = true;
	EXPECT_FALSE(ShouldStop(property, false, false)) << "players still there";
	property.players = 0;
	EXPECT_TRUE(ShouldStop(property, false, false));
	EXPECT_TRUE(ShouldStop(property, true, false)) << "a property is never kept, even in a kept zone";
	EXPECT_FALSE(ShouldStop(property, false, true)) << "someone on the way";
	property.reserved = 1;
	EXPECT_FALSE(ShouldStop(property, false, false)) << "a seat held for someone moving in";
	property.reserved = 0;
	property.outdated = false;
	EXPECT_FALSE(ShouldStop(property, false, false)) << "not outdated";
}

TEST(OutdatedInstancesTest, LeavesWhatTheUpdateHandlesAlone) {
	auto world = View(1100, 1, 0);
	world.outdated = true;
	EXPECT_TRUE(ShouldStop(world, false, false));
	EXPECT_FALSE(ShouldStop(world, true, false)) << "a kept zone's public instance gets its replacement first";
	auto privateWorld = world;
	privateWorld.isPrivate = true;
	EXPECT_TRUE(ShouldStop(privateWorld, true, false));
	auto charSelect = View(0, 2, 0);
	charSelect.outdated = true;
	EXPECT_FALSE(ShouldStop(charSelect, false, false));
	auto draining = world;
	draining.draining = true;
	EXPECT_FALSE(ShouldStop(draining, false, false)) << "being emptied by a move";
	auto stopping = world;
	stopping.shuttingDown = true;
	EXPECT_FALSE(ShouldStop(stopping, false, false)) << "already stopping";
}

TEST(OutdatedInstancesTest, SamePropertyWaitsForTheOldInstance) {
	std::vector<InstanceView> running{ View(1150, 1, 2, 42), View(1150, 2, 1, 43), View(1100, 3, 4) };
	EXPECT_FALSE(MustWaitForOld(running, 1150, 42)) << "the old one isn't outdated";
	running[0].outdated = true;
	EXPECT_TRUE(MustWaitForOld(running, 1150, 42));
	running[0].shuttingDown = true;
	EXPECT_TRUE(MustWaitForOld(running, 1150, 42)) << "still saving on its way out";
	EXPECT_FALSE(MustWaitForOld(running, 1150, 43)) << "another property";
	EXPECT_FALSE(MustWaitForOld(running, 1151, 42)) << "another zone";
	running[2].outdated = true;
	EXPECT_FALSE(MustWaitForOld(running, 1100, 0)) << "public worlds start a new instance at once";
	running.erase(running.begin());
	EXPECT_FALSE(MustWaitForOld(running, 1150, 42)) << "the old one is gone";
}

TEST(OutdatedInstancesTest, MergesNeverGoIntoAnOutdatedInstance) {
	using namespace InstanceMigration;
	const auto source = View(1100, 1, 2);
	auto old = View(1100, 2, 3);
	auto fresh = View(1100, 3, 1);
	EXPECT_EQ(CheckMergeTarget(old, source), eRefusal::NONE);
	old.outdated = true;
	EXPECT_EQ(CheckMergeTarget(old, source), eRefusal::TARGET_OUTDATED);
	EXPECT_EQ(PickMergeTarget({ old, fresh }, source), std::optional<uint32_t>(3u));
	EXPECT_EQ(PickMergeTarget({ old }, source), std::nullopt);
	for (const auto& suggestion : SuggestMerges({ source, old })) EXPECT_NE(suggestion.targetInstance, 2u);
}
