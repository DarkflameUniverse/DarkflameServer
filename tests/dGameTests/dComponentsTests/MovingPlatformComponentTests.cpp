// Moving platform subcomponents: which one the client picks from the level config, the simple mover as live
// constructed it (a NJ monastery cave counterweight, LOT 16141), and a simple mover's trip timing.
#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <bit>

#include "BitStream.h"
#include "CDClientDatabase.h"
#include "Entity.h"
#include "LDFFormat.h"
#include "MovingPlatformComponent.h"
#include "SimplePhysicsComponent.h"

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

// Live: 1 (subcomponents), 1 (dirty), 0 (no path), 1, type 5, the starting point (the object's position and
// rotation, w first), state 9 (stopped at its desired waypoint), waypoint 0, not reversing, then the ending 0 bit
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
	// 1 (subcomponents), 0 (no path), 1 (one follows), type 6
	EXPECT_EQ(bits.substr(0, 3 + 32), "101" "00000110000000000000000000000000");
	EXPECT_EQ(bits.back(), '0');
}
