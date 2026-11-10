// Moving platform subcomponents: which one the client picks from the level config, the simple mover as live
// constructed it (a NJ monastery cave counterweight, LOT 16141), and a simple mover's trip timing.
#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <bit>

#include "BitStream.h"
#include "CDClientDatabase.h"
#include "Entity.h"
#include "LDFFormat.h"
#include "MovementMessages.h"
#include "MovingPlatformComponent.h"
#include "SimplePhysicsComponent.h"
#include "Zone.h"
#include "dZoneManager.h"

namespace {
	float Float(const uint32_t bits) { return std::bit_cast<float>(bits); }

	std::string Bits(RakNet::BitStream& stream) {
		std::string bits;
		stream.ResetReadPointer();
		bool bit{};
		while (stream.Read(bit)) bits += bit ? '1' : '0';
		return bits;
	}

	std::string HexBits(const std::string& hex, const uint32_t count) {
		std::string bits;
		for (size_t i = 0; i + 1 < hex.size(); i += 2) {
			const auto byte = std::stoi(hex.substr(i, 2), nullptr, 16);
			for (int b = 7; b >= 0; b--) bits += ((byte >> b) & 1) ? '1' : '0';
		}
		return bits.substr(0, count);
	}
}

class MovingPlatformComponentTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:");
		CDClientDatabase::ExecuteDML("CREATE TABLE MovingPlatforms (id INTEGER, platformIsSimpleMover INTEGER, platformMoveX REAL, platformMoveY REAL, platformMoveZ REAL, platformMoveTime REAL, platformStartAtEnd INTEGER, description TEXT);");
		CDClientDatabase::ExecuteDML("INSERT INTO MovingPlatforms VALUES (41, 1, 0.0, -2.5, 0.0, 1.0, 0, 'Counterweights in the caves');");
	}
	void TearDown() override { TearDownDependencies(); }
};

// LWOMovingPlatformComponent::LoadConfigData: no setting means a mover without a registry component, a simple mover
// with one; the settings choose otherwise
TEST_F(MovingPlatformComponentTest, SubcomponentTypeFromLevelConfig) {
	Entity pathOnly(288300744895900100, info);
	EXPECT_EQ(ChooseMoverSubComponentType(-1, pathOnly), eMoverSubComponentType::mover);
	EXPECT_EQ(ChooseMoverSubComponentType(0, pathOnly), eMoverSubComponentType::mover);
	EXPECT_EQ(ChooseMoverSubComponentType(41, pathOnly), eMoverSubComponentType::simpleMover);

	info.settings.Insert<bool>(u"platformIsMover", true);
	Entity mover(288300744895900101, info);
	EXPECT_EQ(ChooseMoverSubComponentType(41, mover), eMoverSubComponentType::mover);

	info.settings.values.clear();
	info.settings.Insert<bool>(u"platformIsSimpleMover", true);
	Entity simple(288300744895900102, info);
	EXPECT_EQ(ChooseMoverSubComponentType(-1, simple), eMoverSubComponentType::simpleMover);

	info.settings.values.clear();
	info.settings.Insert<bool>(u"platformIsRotater", true);
	Entity rotater(288300744895900105, info);
	EXPECT_EQ(ChooseMoverSubComponentType(0, rotater), eMoverSubComponentType::rotater);

	// Set, but all false: the client makes no subcomponent
	info.settings.values.clear();
	info.settings.Insert<bool>(u"platformIsMover", false);
	Entity none(288300744895900106, info);
	EXPECT_EQ(ChooseMoverSubComponentType(41, none), eMoverSubComponentType::none);
	info.settings.values.clear();
}

// LWOMovingPlatformComponent::SetupPlatform: one subcomponent per setting that is true, mover, simple mover, rotater
TEST_F(MovingPlatformComponentTest, OneSubcomponentPerSetting) {
	info.settings.Insert<bool>(u"platformIsRotater", true);
	info.settings.Insert<bool>(u"platformIsMover", true);
	Entity both(288300744895900107, info);
	const std::vector expected{ eMoverSubComponentType::mover, eMoverSubComponentType::rotater };
	EXPECT_EQ(ChooseSubComponentTypes(-1, both), expected);
	EXPECT_EQ(ChooseMoverSubComponentType(-1, both), eMoverSubComponentType::mover);
	info.settings.values.clear();
}

// Live: 1 (subcomponents), 1 (dirty), 0 (no path), 1, type 5, the starting point (the object's position and
// rotation, w first), state 9 (waiting at its desired waypoint), waypoint 0, not reversing, then the ending 0 bit
TEST_F(MovingPlatformComponentTest, SimpleMoverConstructionMatchesLive) {
	info.lot = 16141;
	info.pos = NiPoint3(Float(0xc2e02255), Float(0x436d1992), Float(0xc32347e8));
	info.rot = NiQuaternion(Float(0x3f6fa199), 0.0f, Float(0x3eb42497), 0.0f);
	Entity counterweight(288300744895900103, info);
	auto* const platform = counterweight.AddComponent<MovingPlatformComponent>(41, "");
	ASSERT_EQ(platform->GetMoverSubComponentType(), eMoverSubComponentType::simpleMover);
	EXPECT_EQ(platform->GetSimpleMoverSubComponent()->mMove, NiPoint3(0.0f, -2.5f, 0.0f));
	EXPECT_FLOAT_EQ(platform->GetSimpleMoverSubComponent()->mMoveTime, 1.0f);

	RakNet::BitStream stream;
	platform->Serialize(stream, true);
	EXPECT_EQ(Bits(stream), HexBits("d05000000d548b830a4865b50fa11c8f0e6685bcfc000000025c92d0f800000002120000000000000000", 329));

	// Nothing changed: a serialization says so in two bits
	RakNet::BitStream idle;
	platform->Serialize(idle, false);
	EXPECT_EQ(Bits(idle), "00");
}

// Sent to its end it serializes travelling from waypoint 0, and after platformMoveTime it has arrived: stopped at its
// desired and final waypoint 1, moved by platformMove
TEST_F(MovingPlatformComponentTest, SimpleMoverTripTiming) {
	info.lot = 16141;
	info.pos = NiPoint3(10.0f, 20.0f, 30.0f);
	info.rot = QuatUtils::IDENTITY;
	Entity counterweight(288300744895900104, info);
	counterweight.AddComponent<SimplePhysicsComponent>(-1);
	auto* const platform = counterweight.AddComponent<MovingPlatformComponent>(41, "");
	auto* const mover = platform->GetSimpleMoverSubComponent();

	platform->GotoWaypoint(1);
	counterweight.Update(0.0f); // timers added now start on the next update
	EXPECT_EQ(mover->mState, ePlatformStateFlag::Travelling);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 0);
	EXPECT_FALSE(mover->mInReverse);

	RakNet::BitStream sent;
	platform->Serialize(sent, false);
	// 1, not dirty, 1, type 5, no starting point, the state: travelling from 0 forwards, then the ending 0
	EXPECT_EQ(Bits(sent), "101" "00000101000000000000000000000000" "0" "1" "00000010000000000000000000000000" "00000000000000000000000000000000" "0" "0");

	counterweight.Update(0.5f);
	EXPECT_EQ(mover->mState, ePlatformStateFlag::Travelling);
	counterweight.Update(0.6f);
	EXPECT_EQ(mover->mState, ePlatformStateFlag::Stopped | ePlatformStateFlag::ReachedDesiredWaypoint | ePlatformStateFlag::ReachedFinalDestination);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 1);
	EXPECT_EQ(counterweight.GetPosition(), NiPoint3(10.0f, 17.5f, 30.0f));

	// And back
	platform->GotoWaypoint(0);
	counterweight.Update(0.0f);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 1);
	EXPECT_TRUE(mover->mInReverse);
	counterweight.Update(1.1f);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 0);
	EXPECT_EQ(counterweight.GetPosition(), NiPoint3(10.0f, 20.0f, 30.0f));
}

// A rotater's subcomponent is type 6 with a mover's data (LWOPlatform::Deserialize reads both)
TEST_F(MovingPlatformComponentTest, RotaterIsWrittenAsTypeSix) {
	info.settings.Insert<bool>(u"platformIsRotater", true);
	Entity rotater(288300744895900107, info);
	info.settings.values.clear();
	auto* const platform = rotater.AddComponent<MovingPlatformComponent>(-1, "");
	ASSERT_EQ(platform->GetMoverSubComponentType(), eMoverSubComponentType::rotater);
	ASSERT_NE(platform->GetMoverSubComponent(), nullptr);
	platform->SetSerialized(true);

	RakNet::BitStream stream;
	platform->Serialize(stream, true);
	const auto bits = Bits(stream);
	// 1 (subcomponents), 1 (path written with the construction), 0 (no path), 1 (one follows), type 6
	EXPECT_EQ(bits.substr(0, 4 + 32), "1101" "00000110000000000000000000000000");
	EXPECT_EQ(bits.back(), '0');
}

// A mover runs on the server as live ran it: it waits at its first waypoint (9: waiting at a waypoint, as the GF
// shooting gallery targets were constructed), travels after the waypoint's wait (2), waits at the other end (25, turned
// round on a bounce path); sent to a waypoint to stop it waits with the desired waypoint, leaves and stops there (28)
TEST_F(MovingPlatformComponentTest, MoverRunsLikeLive) {
	Path path;
	path.pathType = PathType::MovingPlatform;
	path.pathName = "TestMoverPath";
	path.pathBehavior = PathBehavior::Bounce;
	PathWaypoint first;
	first.position = NiPoint3(0.0f, 0.0f, 0.0f);
	first.speed = 5.0f;
	first.movingPlatform.wait = 2.0f;
	PathWaypoint second = first;
	second.position = NiPoint3(10.0f, 0.0f, 0.0f);
	second.movingPlatform.wait = 1.0f;
	path.pathWaypoints = { first, second };
	const_cast<Zone*>(Game::zoneManager->GetZone())->AddPath(path);

	Entity entity(288300744895900108, info);
	auto* const platform = entity.AddComponent<MovingPlatformComponent>(-1, "TestMoverPath");
	ASSERT_TRUE(platform->IsMover());
	auto* const mover = platform->GetMoverSubComponent();
	EXPECT_EQ(mover->mState, 9u);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 0u);
	EXPECT_EQ(mover->mNextWaypointIndex, 1u);

	platform->Run(1.9f);
	EXPECT_EQ(mover->mState, 9u);
	EXPECT_FLOAT_EQ(mover->mIdleTimeElapsed, 1.9f);
	platform->Run(0.2f);
	EXPECT_EQ(mover->mState, 2u);

	// 5 units a second over 10
	platform->Run(1.0f);
	EXPECT_NEAR(mover->mPercentBetweenPoints, 0.5f, 0.001f);
	EXPECT_NEAR(mover->mPosition.x, 5.0f, 0.01f);
	GameMessages::PlatformResync resync;
	platform->FillResync(resync);
	EXPECT_EQ(static_cast<uint32_t>(resync.eState), 2u);
	EXPECT_EQ(resync.iIndex, 0);
	EXPECT_EQ(resync.iNextIndex, 1);
	EXPECT_NEAR(resync.fPercentBetweenPoints, 0.5f, 0.001f);

	platform->Run(1.1f);
	EXPECT_EQ(mover->mState, 25u);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 1u);
	EXPECT_EQ(mover->mNextWaypointIndex, 0u);
	EXPECT_TRUE(mover->mInReverse);
	EXPECT_EQ(mover->mPercentBetweenPoints, 0.0f);

	// Sent back to the start to stop there: it waits out the waypoint's wait, leaves and stays
	platform->GotoWaypoint(0, true);
	EXPECT_EQ(mover->mState, 25u);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 0);
	EXPECT_TRUE(mover->mShouldStopAtDesiredWaypoint);
	platform->Run(1.1f);
	EXPECT_EQ(mover->mState, 2u);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 0);
	platform->Run(2.5f);
	EXPECT_EQ(mover->mState, 28u);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 0u);
	EXPECT_EQ(mover->mDesiredWaypointIndex, -1);
	platform->Run(10.0f);
	EXPECT_EQ(mover->mState, 28u);

	// Started at a waypoint it first waits there (issue 964), then stopped mid way (4, the bit the client stops on) and
	// started again
	platform->StartPathing();
	EXPECT_EQ(mover->mState, 25u);
	platform->Run(2.1f);
	EXPECT_EQ(mover->mState, 2u);
	platform->Run(1.0f);
	platform->StopPathing();
	EXPECT_EQ(mover->mState, 4u);
	const auto percent = mover->mPercentBetweenPoints;
	platform->Run(1.0f);
	EXPECT_EQ(mover->mPercentBetweenPoints, percent);
	platform->StartPathing();
	EXPECT_EQ(mover->mState, 2u);
}

// The NJ falling tiles in the live captures (LOT 14335, a two waypoint path run once, not started on load): 12 until
// stepped on, sent to waypoint 1 it waits (9) and leaves (2); there it has stopped and turned round (28), and sent back
// it waits (25, reversed, towards 0) and leaves (2), to stop at the start as live constructed used tiles (28, forwards)
TEST_F(MovingPlatformComponentTest, MoverOnceLikeLiveFallingTile) {
	Path path;
	path.pathType = PathType::MovingPlatform;
	path.pathName = "TestDropPath";
	path.pathBehavior = PathBehavior::Once;
	PathWaypoint first;
	first.position = NiPoint3(0.0f, 10.0f, 0.0f);
	first.speed = 5.0f;
	PathWaypoint second = first;
	second.position = NiPoint3(0.0f, 0.0f, 0.0f);
	path.pathWaypoints = { first, second };
	const_cast<Zone*>(Game::zoneManager->GetZone())->AddPath(path);

	info.settings.Insert<bool>(u"startPathingOnLoad", false);
	Entity entity(288300744895900109, info);
	info.settings.values.clear();
	auto* const platform = entity.AddComponent<MovingPlatformComponent>(-1, "TestDropPath");
	auto* const mover = platform->GetMoverSubComponent();
	EXPECT_EQ(mover->mState, 12u);
	platform->Run(5.0f);
	EXPECT_EQ(mover->mState, 12u);

	platform->GotoWaypoint(1);
	EXPECT_EQ(mover->mState, 9u);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 1);
	EXPECT_TRUE(mover->mShouldStopAtDesiredWaypoint);
	platform->Run(0.11f);
	EXPECT_EQ(mover->mState, 2u);
	platform->Run(1.0f);
	EXPECT_EQ(mover->mState, 2u);
	platform->Run(1.1f);
	EXPECT_EQ(mover->mState, 28u);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 1u);
	EXPECT_EQ(mover->mNextWaypointIndex, 0u);
	EXPECT_TRUE(mover->mInReverse);
	EXPECT_EQ(mover->mDesiredWaypointIndex, -1);

	platform->GotoWaypoint(0);
	EXPECT_EQ(mover->mState, 25u);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 0);
	platform->Run(0.11f);
	EXPECT_EQ(mover->mState, 2u);
	platform->Run(2.1f);
	EXPECT_EQ(mover->mState, 28u);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 0u);
	EXPECT_EQ(mover->mNextWaypointIndex, 1u);
	EXPECT_FALSE(mover->mInReverse);
}

// The object goes with its mover on the server (LWOPlatform moves it ignoring the dirty flags), turned between the
// waypoints' rotations, and its physics are not serialized for it: live sent a moving target's mover data only
TEST_F(MovingPlatformComponentTest, MoverCarriesItsObject) {
	Path path;
	path.pathType = PathType::MovingPlatform;
	path.pathName = "TestCarryPath";
	path.pathBehavior = PathBehavior::Once;
	PathWaypoint first;
	first.position = NiPoint3(0.0f, 0.0f, 0.0f);
	first.speed = 5.0f;
	PathWaypoint second = first;
	second.position = NiPoint3(10.0f, 0.0f, 0.0f);
	second.rotation = NiQuaternion(0.0f, 0.0f, 1.0f, 0.0f);
	path.pathWaypoints = { first, second };
	const_cast<Zone*>(Game::zoneManager->GetZone())->AddPath(path);

	info.pos = NiPoint3(0.0f, 0.0f, 0.0f);
	Entity entity(288300744895900110, info);
	auto* const simplePhysics = entity.AddComponent<SimplePhysicsComponent>(-1);
	auto* const platform = entity.AddComponent<MovingPlatformComponent>(-1, "TestCarryPath");

	RakNet::BitStream before;
	simplePhysics->Serialize(before, false);
	platform->Run(0.11f);
	platform->Run(1.0f);
	EXPECT_NEAR(entity.GetPosition().x, 5.0f, 0.01f);
	EXPECT_NEAR(std::abs(entity.GetRotation().y), 0.7071f, 0.001f);

	RakNet::BitStream moved;
	simplePhysics->Serialize(moved, false);
	// No position and rotation (seven floats) in it
	EXPECT_LT(moved.GetNumberOfBitsUsed(), 7u * 32u);

	platform->Run(1.1f);
	EXPECT_EQ(entity.GetPosition(), NiPoint3(10.0f, 0.0f, 0.0f));
	EXPECT_NEAR(std::abs(entity.GetRotation().y), 1.0f, 0.001f);
}

// SetCurrentPath (live's shooting gallery script puts its moving platform targets on their paths with it): the mover
// starts over at the path's waypoint, waiting there as live constructed the targets (9), and the path is written once
TEST_F(MovingPlatformComponentTest, SetCurrentPathPutsTheMoverOnThePath) {
	Path path;
	path.pathType = PathType::MovingPlatform;
	path.pathName = "TestTargetPath";
	path.pathBehavior = PathBehavior::Once;
	PathWaypoint first;
	first.position = NiPoint3(1.0f, 2.0f, 3.0f);
	first.speed = 10.0f;
	PathWaypoint second = first;
	second.position = NiPoint3(1.0f, 2.0f, 23.0f);
	PathWaypoint third = first;
	third.position = NiPoint3(1.0f, 2.0f, 43.0f);
	path.pathWaypoints = { first, second, third };
	const_cast<Zone*>(Game::zoneManager->GetZone())->AddPath(path);

	Entity entity(288300744895900111, info);
	entity.AddComponent<SimplePhysicsComponent>(-1);
	auto* const platform = entity.AddComponent<MovingPlatformComponent>(0, "");
	ASSERT_EQ(platform->GetMoverSubComponentType(), eMoverSubComponentType::mover);
	EXPECT_FALSE(platform->IsMover());
	EXPECT_FALSE(platform->SetCurrentPath("NoSuchPath"));

	ASSERT_TRUE(platform->SetCurrentPath("TestTargetPath", 0));
	ASSERT_TRUE(platform->IsMover());
	auto* const mover = platform->GetMoverSubComponent();
	EXPECT_EQ(mover->mState, 9u);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 0u);
	EXPECT_EQ(mover->mNextWaypointIndex, 1u);
	EXPECT_EQ(entity.GetPosition(), NiPoint3(1.0f, 2.0f, 3.0f));

	// 1 (subcomponents), 1 (the path changed), 1 (there is one), its name's length
	RakNet::BitStream withPath;
	platform->Serialize(withPath, false);
	EXPECT_EQ(Bits(withPath).substr(0, 3 + 16), "111" "0000111000000000");
	RakNet::BitStream withoutPath;
	platform->Serialize(withoutPath, false);
	EXPECT_EQ(Bits(withoutPath).substr(0, 2), "10");

	// It leaves after the shortest wait and passes the middle waypoint waiting there (9), as the live targets did
	platform->Run(0.11f);
	EXPECT_EQ(mover->mState, 2u);
	platform->Run(2.1f);
	EXPECT_EQ(mover->mState, 9u);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 1u);
	platform->Run(0.11f);
	EXPECT_EQ(mover->mState, 2u);
	platform->Run(2.1f);
	EXPECT_EQ(mover->mState, 28u);
	EXPECT_EQ(mover->mCurrentWaypointIndex, 2u);
}
