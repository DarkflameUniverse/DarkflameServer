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

// A civilian's character component: live always wrote the GM, current-activity and social blocks on construction
// (9,726 LOT 1 constructions: is_gm false, gm_level 0, current_activity Some(None), social_info always Some with guild 0
// and an empty guild name). DLU wrote them only after something dirtied them, so a civilian got none of them.
TEST_F(ReplicaConstructionTest, CharacterConstructionAlwaysWritesGmActivityAndSocialBlocks) {
	User user(UNASSIGNED_SYSTEM_ADDRESS, "tester", "key");
	Character character(1, &user);
	info.lot = 1;
	Entity player(1152921506064087003, info);
	player.SetCharacter(&character);
	character.SetEntity(&player);
	auto* const component = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);

	RakNet::BitStream construction;
	component->Serialize(construction, true);

	// Everything before the GM block: 4 absent claim codes, 10 u32 appearance fields, 4 u64s (account, last logout,
	// prop mod display time, u-score), the free-trial bit, 27 u64 statistics and the 2-bit transition state.
	constexpr uint32_t beforeGm = 4 + 10 * 32 + 4 * 64 + 1 + 27 * 64 + 2;
	RakNet::BitStream tail;
	Tail(construction, beforeGm, tail);

	RakNet::BitStream expected;
	expected.Write1(); // gm_pvp_info Some
	expected.Write0(); // pvp_enabled
	expected.Write0(); // is_gm
	expected.Write<uint8_t>(0); // gm_level
	expected.Write0(); // editor_enabled
	expected.Write<uint8_t>(0); // editor_level
	expected.Write1(); // current_activity Some
	expected.Write<uint32_t>(0); // GameActivity::None
	expected.Write1(); // social_info Some
	expected.Write<LWOOBJID>(0); // guild_id
	expected.Write<uint8_t>(0); // guild_name ""
	expected.Write1(); // is_lego_club_member (DLU treats everyone as a member)
	expected.Write<uint32_t>(0); // country code
	ExpectSameBits(tail, expected);

	// A serialization with nothing changed still writes none of them
	RakNet::BitStream serialization;
	component->Serialize(serialization, false);
	EXPECT_EQ(serialization.GetNumberOfBitsUsed(), 3u);

	player.SetCharacter(nullptr);
}
