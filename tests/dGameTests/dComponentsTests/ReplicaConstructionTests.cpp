// Object construction data as live sent it. The expected bits are built field by field in the order the client reads
// them (lu_packets replica structs), with the values of live constructions from the 2011/2012 captures.
#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "BitStream.h"
#include "CDClientDatabase.h"
#include "CDComponentsRegistryTable.h"
#include "CDInventoryComponentTable.h"
#include "CDItemComponentTable.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "ItemComponent.h"
#include "Item.h"
#include "ModelComponent.h"
#include "MovingPlatformComponent.h"
#include "SimplePhysicsComponent.h"
#include "User.h"
#include "eReplicaComponentType.h"
#include "eReplicaPacketType.h"

namespace {
	// Copies the bits of `stream` from bit `from` on into `tail`
	void Tail(RakNet::BitStream& stream, const uint32_t from, RakNet::BitStream& tail) {
		stream.SetReadOffset(from);
		const auto bits = stream.GetNumberOfBitsUsed() - from;
		std::vector<uint8_t> buffer(BITS_TO_BYTES(bits) + 1);
		stream.ReadBits(buffer.data(), bits, false);
		tail.WriteBits(buffer.data(), bits, false);
	}

	// The stream's bits as '0'/'1' characters, so a failure shows where they differ
	std::string Bits(const RakNet::BitStream& stream) {
		std::string bits;
		for (uint32_t i = 0; i < stream.GetNumberOfBitsUsed(); ++i) bits += (stream.GetData()[i / 8] & (0x80 >> (i % 8))) ? '1' : '0';
		return bits;
	}

	void ExpectSameBits(const RakNet::BitStream& actual, const RakNet::BitStream& expected) {
		EXPECT_EQ(Bits(actual), Bits(expected));
	}
}

class ReplicaConstructionTest : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

// time_since_created_on_server: live sent each object's age on the server in milliseconds (0 for the local player's own
// construction right after it was made, the zone's uptime for objects loaded with the zone). DLU sent 0.
TEST_F(ReplicaConstructionTest, TimeSinceCreatedOnServerIsTheObjectsAge) {
	info.lot = 12266;
	Entity entity(288300744895979394, info);

	const auto readTime = [&entity]() {
		RakNet::BitStream stream;
		entity.WriteBaseReplicaData(stream, eReplicaPacketType::CONSTRUCTION);
		LWOOBJID objectID{};
		LOT lot{};
		uint8_t nameLength{};
		uint32_t time{};
		EXPECT_TRUE(stream.Read(objectID));
		EXPECT_TRUE(stream.Read(lot));
		EXPECT_TRUE(stream.Read(nameLength));
		EXPECT_EQ(nameLength, 0);
		EXPECT_TRUE(stream.Read(time));
		EXPECT_EQ(objectID, 288300744895979394);
		EXPECT_EQ(lot, 12266);
		return time;
	};

	EXPECT_LT(readTime(), 1000u);
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	const auto later = readTime();
	EXPECT_GE(later, 30u);
	EXPECT_LT(later, 60000u);
	EXPECT_GE(entity.GetTimeSinceCreatedMs(), later);
}
