#include <gtest/gtest.h>
#include "InstanceMigration.h"

using namespace InstanceMigration;

namespace {
	InstanceView Instance(uint32_t zone, uint32_t instance, int32_t players, int32_t soft = 8, int32_t hard = 12) {
		InstanceView view;
		view.zoneId = zone;
		view.instanceId = instance;
		view.players = players;
		view.softCap = soft;
		view.hardCap = hard;
		view.ready = true;
		return view;
	}
}

TEST(InstanceMigrationTest, SourcesThatCantMove) {
	EXPECT_EQ(CheckSource(Instance(1100, 1, 3), false), eRefusal::NONE);
	EXPECT_EQ(CheckSource(Instance(0, 1, 3), false), eRefusal::CHARACTER_SELECT);
	EXPECT_EQ(CheckSource(Instance(1203, 1, 3), true), eRefusal::ACTIVITY_ZONE);

	auto property = Instance(1150, 2, 1);
	property.cloneId = 55;
	EXPECT_EQ(CheckSource(property, false), eRefusal::PROPERTY_OR_CLONE);

	auto privateInstance = Instance(1100, 3, 1);
	privateInstance.isPrivate = true;
	EXPECT_EQ(CheckSource(privateInstance, false), eRefusal::PRIVATE_INSTANCE);

	auto draining = Instance(1100, 4, 1);
	draining.draining = true;
	EXPECT_EQ(CheckSource(draining, false), eRefusal::ALREADY_MIGRATING);

	auto starting = Instance(1100, 5, 0);
	starting.ready = false;
	EXPECT_EQ(CheckSource(starting, false), eRefusal::NOT_READY);

	auto stopping = Instance(1100, 6, 2);
	stopping.shuttingDown = true;
	EXPECT_EQ(CheckSource(stopping, false), eRefusal::SHUTTING_DOWN);
}

TEST(InstanceMigrationTest, MergeTargetMustFitEveryone) {
	const auto source = Instance(1100, 1, 5);
	EXPECT_EQ(CheckMergeTarget(Instance(1100, 2, 7), source), eRefusal::NONE); // 12 = hard cap
	EXPECT_EQ(CheckMergeTarget(Instance(1100, 2, 8), source), eRefusal::TARGET_FULL);
	EXPECT_EQ(CheckMergeTarget(Instance(1100, 1, 5), source), eRefusal::TARGET_IS_SOURCE);
	EXPECT_EQ(CheckMergeTarget(Instance(1200, 2, 0), source), eRefusal::TARGET_OTHER_ZONE);

	// Seats held for another move count
	auto busy = Instance(1100, 3, 4);
	busy.reserved = 4;
	EXPECT_EQ(CheckMergeTarget(busy, source), eRefusal::TARGET_FULL);

	auto draining = Instance(1100, 4, 0);
	draining.draining = true;
	EXPECT_EQ(CheckMergeTarget(draining, source), eRefusal::ALREADY_MIGRATING);
}

TEST(InstanceMigrationTest, PickMergeTargetPrefersFullestUnderSoftCap) {
	const auto source = Instance(1100, 1, 2);
	const std::vector<InstanceView> instances = {
		source,
		Instance(1100, 2, 3),  // 5 after: fits the soft cap
		Instance(1100, 3, 6),  // 8 after: fits the soft cap, fuller
		Instance(1100, 4, 9),  // 11 after: only under the hard cap
		Instance(1200, 5, 1),  // another zone
	};
	EXPECT_EQ(PickMergeTarget(instances, source), 3u);

	// When nothing stays under the soft cap, the hard cap still allows it
	const std::vector<InstanceView> crowded = { source, Instance(1100, 4, 9), Instance(1100, 6, 10) };
	EXPECT_EQ(PickMergeTarget(crowded, source), 6u);

	// Nobody has room
	const std::vector<InstanceView> full = { source, Instance(1100, 7, 11) };
	EXPECT_FALSE(PickMergeTarget(full, source).has_value());

	// Equal loads: the older instance
	const std::vector<InstanceView> tie = { source, Instance(1100, 9, 3), Instance(1100, 8, 3) };
	EXPECT_EQ(PickMergeTarget(tie, source), 8u);
}

TEST(InstanceMigrationTest, SuggestMergesPairsQuietInstances) {
	const std::vector<InstanceView> instances = {
		Instance(1100, 1, 1),
		Instance(1100, 2, 2),
		Instance(1100, 3, 6),
		Instance(1200, 4, 3),  // alone in its zone
		Instance(1300, 5, 0),  // empty: shuts itself down
		Instance(1300, 6, 2),
	};
	const auto suggestions = SuggestMerges(instances);
	// 1 goes into the fullest that fits (3: 6 + 1 = 7); 2 no longer fits there (7 + 2 > 8). Zone 1300's only
	// non-empty instance isn't moved into an empty one (that one shuts itself down anyway)
	ASSERT_EQ(suggestions.size(), 1u);
	EXPECT_EQ(suggestions[0].zoneId, 1100u);
	EXPECT_EQ(suggestions[0].sourceInstance, 1u);
	EXPECT_EQ(suggestions[0].targetInstance, 3u);
	EXPECT_EQ(suggestions[0].players, 1);
	EXPECT_EQ(suggestions[0].resulting, 7);
}

TEST(InstanceMigrationTest, SuggestMergesSkipsWhatCantMove) {
	auto property = Instance(1150, 1, 1);
	property.cloneId = 9;
	auto otherProperty = Instance(1150, 2, 1);
	otherProperty.cloneId = 10;
	auto draining = Instance(1100, 3, 1);
	draining.draining = true;
	EXPECT_TRUE(SuggestMerges({ property, otherProperty, draining, Instance(1100, 4, 2) }).empty());
}

TEST(InstanceMigrationTest, BusyPlayersWaitThenGo) {
	EXPECT_EQ(DecidePlayer(false, false, false, 0.0f, 15.0f), ePlayerDecision::MOVE);
	EXPECT_EQ(DecidePlayer(true, false, false, 0.0f, 15.0f), ePlayerDecision::CANCEL_TRADE_MOVE);
	EXPECT_EQ(DecidePlayer(false, true, false, 1.0f, 15.0f), ePlayerDecision::WAIT);
	EXPECT_EQ(DecidePlayer(false, false, true, 1.0f, 15.0f), ePlayerDecision::WAIT);
	EXPECT_EQ(DecidePlayer(false, false, true, 15.0f, 15.0f), ePlayerDecision::MOVE);
	EXPECT_EQ(DecidePlayer(true, true, false, 20.0f, 15.0f), ePlayerDecision::CANCEL_TRADE_MOVE);
}

TEST(InstanceMigrationTest, RequestRoundTrip) {
	InstanceMigrationRequest request;
	request.requestId = 77;
	request.kind = eKind::MERGE;
	request.zoneId = 1100;
	request.sourceInstance = 4;
	request.targetInstance = 2;
	request.warnSeconds = 30;
	request.shutdownSource = false;
	request.seamless = true;
	request.requesterId = 1152921510436607007LL;
	request.requestedBy = "admin";

	RakNet::BitStream stream;
	request.Serialize(stream);
	InstanceMigrationRequest read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.requestId, 77u);
	EXPECT_EQ(read.kind, eKind::MERGE);
	EXPECT_EQ(read.zoneId, 1100u);
	EXPECT_EQ(read.sourceInstance, 4u);
	EXPECT_EQ(read.targetInstance, 2u);
	EXPECT_EQ(read.warnSeconds, 30u);
	EXPECT_FALSE(read.shutdownSource);
	EXPECT_TRUE(read.seamless);
	EXPECT_EQ(read.requesterId, 1152921510436607007LL);
	EXPECT_EQ(read.requestedBy, "admin");
}

TEST(InstanceMigrationTest, RequestRejectsBadValues) {
	RakNet::BitStream stream;
	stream.Write<uint32_t>(1);
	stream.Write<uint8_t>(7); // no such kind
	stream.Write<uint32_t>(1100);
	stream.Write<uint32_t>(1);
	stream.Write<uint32_t>(0);
	stream.Write<uint16_t>(10);
	stream.Write<uint8_t>(1);
	stream.Write<uint8_t>(0);
	stream.Write<LWOOBJID>(0);
	stream.Write<uint16_t>(0);
	InstanceMigrationRequest read;
	EXPECT_FALSE(read.Deserialize(stream));

	RakNet::BitStream truncated;
	truncated.Write<uint32_t>(1);
	EXPECT_FALSE(read.Deserialize(truncated));
}

TEST(InstanceMigrationTest, OrderRoundTrip) {
	MigratePlayersOrder order;
	order.migrationId = 5;
	order.targetZone = 1200;
	order.targetInstance = 9;
	order.targetClone = 0;
	order.targetIp = "10.0.0.2";
	order.targetPort = 3003;
	order.warnSeconds = 10;
	order.playersPerSecond = 4;
	order.mythranShift = false;
	order.seamless = true;

	RakNet::BitStream stream;
	order.Serialize(stream);
	MigratePlayersOrder read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.migrationId, 5u);
	EXPECT_EQ(read.targetZone, 1200u);
	EXPECT_EQ(read.targetInstance, 9u);
	EXPECT_EQ(read.targetIp, "10.0.0.2");
	EXPECT_EQ(read.targetPort, 3003u);
	EXPECT_EQ(read.warnSeconds, 10u);
	EXPECT_EQ(read.playersPerSecond, 4u);
	EXPECT_FALSE(read.mythranShift);
	EXPECT_TRUE(read.seamless);
}

TEST(InstanceMigrationTest, StatusRoundTrip) {
	MigrationStatus status;
	status.migrationId = 12;
	status.state = eState::MOVING;
	status.kind = eKind::REPLACE;
	status.zoneId = 1100;
	status.sourceInstance = 1;
	status.targetInstance = 7;
	status.moved = 3;
	status.remaining = 2;
	status.failed = 1;
	status.requesterId = 42;
	status.message = "Moving players";

	RakNet::BitStream stream;
	status.Serialize(stream);
	MigrationStatus read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.migrationId, 12u);
	EXPECT_EQ(read.state, eState::MOVING);
	EXPECT_EQ(read.kind, eKind::REPLACE);
	EXPECT_EQ(read.targetInstance, 7u);
	EXPECT_EQ(read.moved, 3u);
	EXPECT_EQ(read.remaining, 2u);
	EXPECT_EQ(read.failed, 1u);
	EXPECT_EQ(read.requesterId, 42);
	EXPECT_EQ(read.message, "Moving players");
	EXPECT_FALSE(read.Finished());
	read.state = eState::FAILED;
	EXPECT_TRUE(read.Finished());
}

TEST(InstanceMigrationTest, CarriedStateRoundTrip) {
	CarriedPlayerState state;
	state.targetZone = 1100;
	state.targetInstance = 3;
	state.characterId = 1152921510436607007LL;
	state.petItemId = 1152921510436607100LL;
	state.seamless = true;

	RakNet::BitStream stream;
	state.Serialize(stream);
	CarriedPlayerState read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.targetZone, 1100u);
	EXPECT_EQ(read.targetInstance, 3u);
	EXPECT_EQ(read.characterId, 1152921510436607007LL);
	EXPECT_EQ(read.petItemId, 1152921510436607100LL);
	EXPECT_TRUE(read.seamless);
}
