#include "PropertyBuilders.h"
#include "BrickByBrick.h"

#include <gtest/gtest.h>

// Who builds on a property and where models taken off it go (docs/PropertyBuilding.md).
namespace {
	constexpr LWOOBJID OWNER = 1152921510000000001LL;
	constexpr LWOOBJID FRIEND = 1152921510000000002LL;
	constexpr LWOOBJID VISITOR = 1152921510000000003LL;

	int32_t Reason(const BrickByBrick::eDeleteReason reason) { return static_cast<int32_t>(reason); }

	bool CanBuild(const LWOOBJID id, const bool isBestFriend, const bool isBuilding, const bool ownerBuilding, const bool bestFriendsBuild, const LWOOBJID owner = OWNER) {
		return PropertyBuilders::CanBuild({ .id = id, .isBestFriend = isBestFriend, .isBuilding = isBuilding }, owner, ownerBuilding, bestFriendsBuild);
	}
}

TEST(PropertyBuildersTests, OnlyTheOwnerBuildsByDefault) {
	EXPECT_TRUE(CanBuild(OWNER, false, false, false, false));
	EXPECT_FALSE(CanBuild(FRIEND, true, false, true, false));
	EXPECT_FALSE(CanBuild(FRIEND, true, true, true, false));
	EXPECT_FALSE(CanBuild(VISITOR, false, false, true, false));
}

TEST(PropertyBuildersTests, NobodyBuildsOnAnUnclaimedProperty) {
	EXPECT_FALSE(CanBuild(FRIEND, true, true, true, true, LWOOBJID_EMPTY));
	EXPECT_FALSE(CanBuild(LWOOBJID_EMPTY, false, false, false, true, LWOOBJID_EMPTY));
}

TEST(PropertyBuildersTests, TheOwnerStartsBuilding) {
	// A best friend can't start: the owner isn't building
	EXPECT_FALSE(CanBuild(FRIEND, true, false, false, true));
	// The owner always can
	EXPECT_TRUE(CanBuild(OWNER, false, false, false, true));
}

TEST(PropertyBuildersTests, BestFriendsJoinWhileTheOwnerBuilds) {
	EXPECT_TRUE(CanBuild(FRIEND, true, false, true, true));
	EXPECT_TRUE(CanBuild(FRIEND, true, true, true, true));
	// Not a best friend: never
	EXPECT_FALSE(CanBuild(VISITOR, false, false, true, true));
	EXPECT_FALSE(CanBuild(VISITOR, false, true, true, true));
}

TEST(PropertyBuildersTests, BestFriendsStayAfterTheOwnerStops) {
	// Still in build mode: keeps building
	EXPECT_TRUE(CanBuild(FRIEND, true, true, false, true));
	// Left build mode: can't come back until the owner builds again
	EXPECT_FALSE(CanBuild(FRIEND, true, false, false, true));
}

TEST(PropertyBuildersTests, ModelsWithNoPlacerAreTheOwners) {
	EXPECT_EQ(PropertyBuilders::Placer(LWOOBJID_EMPTY, OWNER), OWNER);
	EXPECT_EQ(PropertyBuilders::Placer(FRIEND, OWNER), FRIEND);
	EXPECT_EQ(PropertyBuilders::Placer(OWNER, OWNER), OWNER);
}

TEST(PropertyBuildersTests, APlayersOwnModelGoesToThem) {
	using enum BrickByBrick::eDeleteReason;
	using enum PropertyBuilders::eModelReturn;
	for (const auto reason : { PICKING_MODEL_UP, RETURNING_MODEL_TO_INVENTORY, BREAKING_MODEL_APART }) {
		EXPECT_EQ(PropertyBuilders::PlanModelReturn(FRIEND, FRIEND, true, Reason(reason)), PICKER);
		EXPECT_EQ(PropertyBuilders::PlanModelReturn(OWNER, OWNER, true, Reason(reason)), PICKER);
	}
}

TEST(PropertyBuildersTests, SomeoneElsesModelGoesBackToThem) {
	using enum BrickByBrick::eDeleteReason;
	using enum PropertyBuilders::eModelReturn;
	EXPECT_EQ(PropertyBuilders::PlanModelReturn(FRIEND, OWNER, true, Reason(PICKING_MODEL_UP)), PLACER);
	EXPECT_EQ(PropertyBuilders::PlanModelReturn(OWNER, FRIEND, true, Reason(RETURNING_MODEL_TO_INVENTORY)), PLACER);
	// Not in this world: it stays placed
	EXPECT_EQ(PropertyBuilders::PlanModelReturn(OWNER, FRIEND, false, Reason(PICKING_MODEL_UP)), PLACER_AWAY);
	EXPECT_EQ(PropertyBuilders::PlanModelReturn(FRIEND, OWNER, false, Reason(RETURNING_MODEL_TO_INVENTORY)), PLACER_AWAY);
}

TEST(PropertyBuildersTests, OnlyThePlacerTakesTheirModelApart) {
	using enum BrickByBrick::eDeleteReason;
	using enum PropertyBuilders::eModelReturn;
	EXPECT_EQ(PropertyBuilders::PlanModelReturn(FRIEND, OWNER, true, Reason(BREAKING_MODEL_APART)), NOT_THEIRS);
	EXPECT_EQ(PropertyBuilders::PlanModelReturn(OWNER, FRIEND, false, Reason(BREAKING_MODEL_APART)), NOT_THEIRS);
}
