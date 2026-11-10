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
#include "LevelProgressionComponent.h"
#include "LUPExhibitComponent.h"
#include "PlayerForcedMovementComponent.h"
#include "ScriptedActivityComponent.h"
#include "Item.h"
#include "ModelComponent.h"
#include "MovingPlatformComponent.h"
#include "PhantomPhysicsComponent.h"
#include "SimplePhysicsComponent.h"
#include "User.h"
#include "eGameActivity.h"
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

// A current-activity or PVP change is written once, as a guild change is: LWOCharacterComponent::Deserialize reads each
// block behind its own flag bit.
TEST_F(ReplicaConstructionTest, CharacterActivityAndPvpChangesAreWrittenOnce) {
	User user(UNASSIGNED_SYSTEM_ADDRESS, "tester", "key");
	Character character(1, &user);
	info.lot = 1;
	Entity player(1152921506064087005, info);
	player.SetCharacter(&character);
	character.SetEntity(&player);
	auto* const component = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);

	component->SetCurrentActivity(eGameActivity::QUICKBUILDING);
	component->SetPvpEnabled(true);
	RakNet::BitStream serialization;
	component->Serialize(serialization, false);
	RakNet::BitStream expected;
	expected.Write1(); // gm_pvp_info
	expected.Write1(); // pvp_enabled
	expected.Write0(); // is_gm
	expected.Write<uint8_t>(0); // gm_level
	expected.Write0(); // editor_enabled
	expected.Write<uint8_t>(0); // editor_level
	expected.Write1(); // current_activity
	expected.Write(eGameActivity::QUICKBUILDING);
	expected.Write0(); // social_info
	ExpectSameBits(serialization, expected);

	RakNet::BitStream again;
	component->Serialize(again, false);
	EXPECT_EQ(again.GetNumberOfBitsUsed(), 3u);

	// A construction in between (another player coming into range) doesn't swallow a pending change
	component->SetCurrentActivity(eGameActivity::NONE);
	RakNet::BitStream construction;
	component->Serialize(construction, true);
	RakNet::BitStream after;
	component->Serialize(after, false);
	EXPECT_EQ(after.GetNumberOfBitsUsed(), 3u + 32u);

	player.SetCharacter(nullptr);
}

// A player in jetpack mode is constructed in it: live player constructions carried the jetpack block (effect 167,
// flying 0, bypass checks 1) and a 0 after the pickup radius. LWOControllablePhysComponent::Deserialize reads both.
TEST_F(ReplicaConstructionTest, ControllablePhysicsConstructionCarriesJetPackMode) {
	Entity player(1152921506064087006, info);
	auto* const physics = player.AddComponent<ControllablePhysicsComponent>(-1);

	const auto construct = [physics]() {
		RakNet::BitStream construction;
		physics->Serialize(construction, true);
		return Bits(construction);
	};
	const auto bits = [](const RakNet::BitStream& stream) { return Bits(stream); };

	// No jetpack: one 0 bit, then the stun immunities
	const auto plain = construct();
	EXPECT_EQ(plain.substr(0, 2), "01");

	physics->SetJetPackMode(true, 167, true);
	RakNet::BitStream head;
	head.Write1(); // jetpack
	head.Write<int32_t>(167); // effect
	head.Write0(); // flying
	head.Write1(); // bypass checks
	head.Write1(); // stun immunities
	const auto jetpack = construct();
	EXPECT_EQ(jetpack.substr(0, head.GetNumberOfBitsUsed()), bits(head));
	// After the 7 immunity counts: no cheats, then the pickup radius and a 0 (not the jetpack flag)
	const size_t afterImmunities = head.GetNumberOfBitsUsed() + 7 * 32;
	EXPECT_EQ(jetpack.substr(afterImmunities, 2), "01");
	EXPECT_EQ(jetpack[afterImmunities + 2 + 32], '0');
	// The rest is what a player without a jetpack gets
	EXPECT_EQ(jetpack.substr(head.GetNumberOfBitsUsed()), plain.substr(2));

	physics->SetJetPackMode(false, -1, false);
	EXPECT_EQ(construct(), plain);
}

namespace {
	struct VolumeEffect {
		uint32_t type{};
		float amount{};
		bool hasDistance{};
		float min{};
		float max{};
		bool isDirectional{};
		NiPoint3 direction{};
	};

	// A phantom volume looks its LOT up in the registry when it is made
	void ConnectRegistry() {
		if (CDClientDatabase::isConnected) return;
		CDClientDatabase::Connect(":memory:");
		CDClientDatabase::ExecuteDML("CREATE TABLE ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER);");
	}

	// The effect block of a phantom physics update made from a trigger's SetPhysicsVolumeEffect arguments
	VolumeEffect ReadVolumeEffect(Entity& volume, const std::vector<std::string>& args) {
		auto* const phantom = volume.AddComponent<PhantomPhysicsComponent>(-1);
		phantom->SetVolumeEffect(args);
		RakNet::BitStream stream;
		phantom->Serialize(stream, false);

		VolumeEffect effect;
		bool flag{};
		stream.Read(flag);
		if (flag) stream.IgnoreBits(7 * 32); // position and rotation
		stream.Read(flag);
		EXPECT_TRUE(flag); // the effect block
		stream.Read(flag);
		EXPECT_TRUE(flag); // active
		stream.Read(effect.type);
		stream.Read(effect.amount);
		stream.Read(effect.hasDistance);
		if (effect.hasDistance) {
			stream.Read(effect.min);
			stream.Read(effect.max);
		}
		stream.Read(effect.isDirectional);
		if (effect.isDirectional) {
			stream.Read(effect.direction.x);
			stream.Read(effect.direction.y);
			stream.Read(effect.direction.z);
		}
		EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
		return effect;
	}
}

// Trigger volume effects as live sent them (phantom physics constructions in the live captures; the client reads
// them in LWOPhantomPhysicsComponent::Deserialize): the direction has the amount as its length, the distances go out
// for "true" only and as 1 and the max, and a zero direction is no direction.
TEST_F(ReplicaConstructionTest, PhantomPhysicsVolumeEffectsAsLive) {
	ConnectRegistry();
	Entity pushWithDistance(1001, info);
	auto effect = ReadVolumeEffect(pushWithDistance, { "Push", "50", "0.91", "-0.34", "-0.22", "true", "20", "50" });
	EXPECT_EQ(effect.type, 0u);
	EXPECT_EQ(effect.amount, 50.0f);
	EXPECT_TRUE(effect.hasDistance);
	EXPECT_EQ(effect.min, 1.0f);
	EXPECT_EQ(effect.max, 50.0f);
	EXPECT_TRUE(effect.isDirectional);
	EXPECT_NEAR(effect.direction.x, 45.68f, 0.01f);
	EXPECT_NEAR(effect.direction.y, -17.07f, 0.01f);
	EXPECT_NEAR(effect.direction.z, -11.04f, 0.01f);

	Entity pushFar(1002, info);
	effect = ReadVolumeEffect(pushFar, { "Push", "200", "1", "0", "0", "true", "150", "175" });
	EXPECT_TRUE(effect.hasDistance);
	EXPECT_EQ(effect.min, 1.0f);
	EXPECT_EQ(effect.max, 175.0f);
	EXPECT_EQ(effect.direction, NiPoint3(200.0f, 0.0f, 0.0f));

	// "1" is not "true": no distances
	Entity updraft(1003, info);
	effect = ReadVolumeEffect(updraft, { "Push", "40.0", "0", "1", "0", "1", "20", "50" });
	EXPECT_EQ(effect.amount, 40.0f);
	EXPECT_FALSE(effect.hasDistance);
	EXPECT_TRUE(effect.isDirectional);
	EXPECT_EQ(effect.direction, NiPoint3(0.0f, 40.0f, 0.0f));

	Entity repulse(1004, info);
	effect = ReadVolumeEffect(repulse, { "Repulse", "150", "0", "0", "0", "true", "25", "100" });
	EXPECT_EQ(effect.type, 2u);
	EXPECT_EQ(effect.amount, 150.0f);
	EXPECT_TRUE(effect.hasDistance);
	EXPECT_EQ(effect.min, 1.0f);
	EXPECT_EQ(effect.max, 100.0f);
	EXPECT_FALSE(effect.isDirectional);

	Entity slanted(1005, info);
	effect = ReadVolumeEffect(slanted, { "Push", " 35", " .79", " 0", " .61" });
	EXPECT_FALSE(effect.hasDistance);
	EXPECT_NEAR(effect.direction.x, 27.70f, 0.01f);
	EXPECT_NEAR(effect.direction.z, 21.39f, 0.01f);

	Entity friction(1006, info);
	effect = ReadVolumeEffect(friction, { "Friction", "0" });
	EXPECT_EQ(effect.type, 4u);
	EXPECT_FALSE(effect.hasDistance);
	EXPECT_FALSE(effect.isDirectional);

	// The amount is a float all the way: LWOPhantomPhysicsComponent::Deserialize (0x00c34db0) reads 32 bits into a
	// float, and trigger commands are run by the server only, so a fraction is kept as the zone file has it
	Entity lowFriction(1007, info);
	effect = ReadVolumeEffect(lowFriction, { "Friction", "0.1" });
	EXPECT_EQ(effect.type, 4u);
	EXPECT_EQ(effect.amount, 0.1f);
}

// A construction (the volume coming into one player's range) doesn't use up an effect change the players already
// watching it still have to get.
TEST_F(ReplicaConstructionTest, PhantomPhysicsConstructionKeepsThePendingEffect) {
	ConnectRegistry();
	Entity volume(1007, info);
	auto* const phantom = volume.AddComponent<PhantomPhysicsComponent>(-1);
	RakNet::BitStream first;
	phantom->Serialize(first, false);
	const auto idle = first.GetNumberOfBitsUsed();

	phantom->SetVolumeEffect({ "Friction", "0" });
	RakNet::BitStream construction;
	phantom->Serialize(construction, true);
	RakNet::BitStream update;
	phantom->Serialize(update, false);
	EXPECT_EQ(update.GetNumberOfBitsUsed(), 2u + 1 + 32 + 32 + 1 + 1);
	RakNet::BitStream again;
	phantom->Serialize(again, false);
	EXPECT_EQ(again.GetNumberOfBitsUsed(), 2u);
	EXPECT_LE(again.GetNumberOfBitsUsed(), idle);
}

// A construction goes to the one client the object came into range of. It must not use up a change the clients
// already watching are still waiting for (the level, rail and billboard flags are each read behind a flag bit).
TEST_F(ReplicaConstructionTest, AConstructionKeepsPendingLevelAndForcedMovementChanges) {
	Entity player(1152921506064087008, info);
	auto* const level = player.AddComponent<LevelProgressionComponent>(-1);
	auto* const forcedMovement = player.AddComponent<PlayerForcedMovementComponent>(-1);

	level->SetLevel(7);
	forcedMovement->SetPlayerOnRail(true);
	RakNet::BitStream construction;
	level->Serialize(construction, true);
	forcedMovement->Serialize(construction, true);

	RakNet::BitStream update;
	level->Serialize(update, false);
	forcedMovement->Serialize(update, false);
	RakNet::BitStream expected;
	expected.Write1();
	expected.Write<uint32_t>(7);
	expected.Write1();
	expected.Write1(); // on rail
	expected.Write0(); // show billboard
	ExpectSameBits(update, expected);

	RakNet::BitStream again;
	level->Serialize(again, false);
	forcedMovement->Serialize(again, false);
	EXPECT_EQ(again.GetNumberOfBitsUsed(), 2u);
}

// The client learns the exhibit's LOT only from the component's block (LWOLUPExhibitComponent::Deserialize: a flag
// and the LOT). DLU wrote it on a construction only while it was dirty, so a player who came into range after the
// first update saw no exhibit until the next one was picked.
TEST_F(ReplicaConstructionTest, LupExhibitConstructionAlwaysCarriesTheExhibit) {
	Entity exhibit(1009, info);
	auto* const component = exhibit.AddComponent<LUPExhibitComponent>(-1);

	RakNet::BitStream update;
	component->Serialize(update, false);
	RakNet::BitStream idle;
	component->Serialize(idle, false);
	EXPECT_EQ(idle.GetNumberOfBitsUsed(), 1u);

	RakNet::BitStream construction;
	component->Serialize(construction, true);
	RakNet::BitStream expected;
	expected.Write1();
	expected.Write<LOT>(11121);
	ExpectSameBits(construction, expected);
}

// A guild change (docs/Guilds.md) is written once: the client reads guild id, a u8 length and 16-bit characters, and
// redraws the name billboard every time it reads a name.
TEST_F(ReplicaConstructionTest, CharacterGuildChangeIsWrittenOnce) {
	User user(UNASSIGNED_SYSTEM_ADDRESS, "tester", "key");
	Character character(1, &user);
	info.lot = 1;
	Entity player(1152921506064087004, info);
	player.SetCharacter(&character);
	character.SetEntity(&player);
	auto* const component = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	RakNet::BitStream construction;
	component->Serialize(construction, true);

	component->SetGuild(7, u"Brick");
	EXPECT_EQ(component->GetGuildID(), 7);
	RakNet::BitStream serialization;
	component->Serialize(serialization, false);
	RakNet::BitStream expected;
	expected.Write0(); // gm_pvp_info
	expected.Write0(); // current_activity
	expected.Write1(); // social_info
	expected.Write<LWOOBJID>(7);
	expected.Write<uint8_t>(5);
	for (const char16_t c : std::u16string(u"Brick")) expected.Write<uint16_t>(c);
	expected.Write1();
	expected.Write<uint32_t>(0);
	ExpectSameBits(serialization, expected);

	RakNet::BitStream again;
	component->Serialize(again, false);
	EXPECT_EQ(again.GetNumberOfBitsUsed(), 3u);
	// The same guild again changes nothing
	component->SetGuild(7, u"Brick");
	RakNet::BitStream same;
	component->Serialize(same, false);
	EXPECT_EQ(same.GetNumberOfBitsUsed(), 3u);

	player.SetCharacter(nullptr);
}

// A client that comes into range of an activity is told its players, whether or not they changed since the last
// update. Live finish line constructions (LOT 4967): flag 1 and the count in 40 of 40, one of them with a player.
TEST_F(ReplicaConstructionTest, AnActivityConstructionAlwaysCarriesItsPlayers) {
	ConnectRegistry();
	Entity activity(1010, info);
	auto* const component = activity.AddComponent<ScriptedActivityComponent>(-1);
	const LWOOBJID player = 1152921506064087006;
	component->SetActivityValue(player, 1, 42.0f);

	RakNet::BitStream update;
	component->Serialize(update, false);
	RakNet::BitStream idle;
	component->Serialize(idle, false);
	EXPECT_EQ(Bits(idle), "0"); // nothing changed since

	RakNet::BitStream construction;
	component->Serialize(construction, true);
	EXPECT_EQ(Bits(construction), Bits(update));
	EXPECT_TRUE(construction.ReadBit());
	uint32_t count{};
	ASSERT_TRUE(construction.Read(count));
	EXPECT_EQ(count, 1u);
	LWOOBJID id{};
	ASSERT_TRUE(construction.Read(id));
	EXPECT_EQ(id, player);
	for (uint32_t i = 0; i < 10; i++) {
		float value{};
		ASSERT_TRUE(construction.Read(value));
		EXPECT_EQ(value, i == 1 ? 42.0f : 0.0f);
	}
	EXPECT_EQ(construction.GetNumberOfUnreadBits(), 0u);

	// Without players: the flag and a count of 0
	component->RemoveActivityPlayerData(player);
	RakNet::BitStream cleared;
	component->Serialize(cleared, false);
	RakNet::BitStream empty;
	component->Serialize(empty, true);
	EXPECT_EQ(Bits(empty), "1" + std::string(32, '0'));
}

// Run speed modifiers stack as in LWOControllablePhysComponent::msgAddRunspeedModifier (0x00be0af0) and
// msgRemoveRunSpeedModifier (0x00be0720): the lowest slow wins, otherwise the highest boost; 500 and anything over 1000
// count for nothing
TEST_F(ReplicaConstructionTest, RunSpeedModifiersStackAsTheClientDoes) {
	Entity entity(15, info);
	auto* const physics = entity.AddComponent<ControllablePhysicsComponent>(-1);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 1.0f);

	physics->AddSpeedboost(750.0f);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 1.5f);
	physics->AddSpeedboost(550.0f); // a smaller boost applied later does not replace the bigger one
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 1.5f);
	physics->AddSpeedboost(250.0f); // any slow beats every boost
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 0.5f);
	physics->AddSpeedboost(133.0f);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 133.0f / 500.0f);
	physics->AddSpeedboost(350.0f);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 133.0f / 500.0f);
	physics->AddSpeedboost(1500.0f);
	physics->AddSpeedboost(500.0f);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 133.0f / 500.0f);

	physics->RemoveSpeedboost(133.0f);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 0.5f);
	physics->RemoveSpeedboost(250.0f);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 0.7f);
	physics->RemoveSpeedboost(350.0f);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 1.5f);
	physics->RemoveSpeedboost(750.0f);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 1.1f);
	physics->RemoveSpeedboost(550.0f);
	EXPECT_FLOAT_EQ(physics->GetSpeedMultiplier(), 1.0f); // 1500 and 500 are left and count for nothing
}

// Live wrote the cheat block (gravity scale, speed multiplier) on construction only when one of them was changed: 216
// of 20,617 controllable-physics constructions had it (players at run speed 1.05, enemies with gravity 0), never 1/1.
TEST_F(ReplicaConstructionTest, ControllablePhysicsConstructionWritesCheatsOnlyWhenChanged) {
	Entity entity(15, info);
	auto* const physics = entity.AddComponent<ControllablePhysicsComponent>(-1);

	const auto cheatBits = [physics]() {
		RakNet::BitStream stream;
		physics->Serialize(stream, true);
		stream.IgnoreBits(1 + 1 + 7 * 32); // no jetpack, stun immunities
		bool hasCheats{};
		EXPECT_TRUE(stream.Read(hasCheats));
		std::pair<float, float> cheats{ -1.0f, -1.0f };
		if (hasCheats) {
			EXPECT_TRUE(stream.Read(cheats.first));
			EXPECT_TRUE(stream.Read(cheats.second));
		}
		return std::make_pair(hasCheats, cheats);
	};

	EXPECT_FALSE(cheatBits().first);

	physics->SetSpeedMultiplier(1.05f);
	const auto [hasCheats, cheats] = cheatBits();
	EXPECT_TRUE(hasCheats);
	EXPECT_FLOAT_EQ(cheats.first, 1.0f);
	EXPECT_FLOAT_EQ(cheats.second, 1.05f);
}

// A client that kept its player across a seamless instance move (docs/SeamlessTransfer.md): its own player's
// construction carries no frame, because LWOBasePhysComponent::Deserialize (0x00bf5a10) applies a construction's
// position, rotation and velocities to the object at once. Everyone else's construction still has it.
TEST_F(ReplicaConstructionTest, ControllablePhysicsConstructionCanLeaveThePositionOut) {
	Entity entity(15, info);
	auto* const physics = entity.AddComponent<ControllablePhysicsComponent>(-1);
	physics->SetPosition(NiPoint3(1.0f, 2.0f, 3.0f));

	// no jetpack, stun immunities, no cheats, equipped item info (flag, pickup radius, in jetpack), bubble (flag, off)
	constexpr uint32_t beforePosition = 1 + 1 + 7 * 32 + 1 + (1 + 32 + 1) + (1 + 1);
	const auto positionBit = [physics, beforePosition](uint32_t& bitsAfter) {
		RakNet::BitStream stream;
		physics->Serialize(stream, true);
		stream.IgnoreBits(beforePosition);
		bool hasPosition{};
		EXPECT_TRUE(stream.Read(hasPosition));
		bitsAfter = stream.GetNumberOfUnreadBits();
		return hasPosition;
	};

	uint32_t bitsAfter{};
	EXPECT_TRUE(positionBit(bitsAfter));
	EXPECT_GT(bitsAfter, 7u * 32u);

	physics->SetKeepClientPosition(true);
	EXPECT_FALSE(positionBit(bitsAfter));
	EXPECT_EQ(bitsAfter, 0u);

	// Serializations are untouched: a moved player is still sent
	RakNet::BitStream flushed;
	physics->Serialize(flushed, false);
	RakNet::BitStream serialization;
	physics->SetPosition(NiPoint3(4.0f, 5.0f, 6.0f));
	physics->Serialize(serialization, false);
	serialization.IgnoreBits(3); // no cheats, equipped item info or bubble change
	bool hasPosition{};
	EXPECT_TRUE(serialization.Read(hasPosition));
	EXPECT_TRUE(hasPosition);

	physics->SetKeepClientPosition(false);
	EXPECT_TRUE(positionBit(bitsAfter));
}

// The same client's character construction: world transition state 3. With 0 the client's
// LWOCharacterComponent::PostCharacterLoaded (0x00c71660) fades the whole screen in from white and pushes its gameplay
// state a second time; 1 is the rocket landing and 2 hides the character. The blocks after it stay where they were.
TEST_F(ReplicaConstructionTest, CharacterConstructionCanSkipTheWorldTransition) {
	User user(UNASSIGNED_SYSTEM_ADDRESS, "tester", "key");
	Character character(1, &user);
	info.lot = 1;
	Entity player(1152921506064087003, info);
	player.SetCharacter(&character);
	character.SetEntity(&player);
	auto* const component = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);

	// 4 absent claim codes, 10 u32 appearance fields, 4 u64s, the free-trial bit and 27 u64 statistics
	constexpr uint32_t beforeTransition = 4 + 10 * 32 + 4 * 64 + 1 + 27 * 64;
	const auto transition = [component, beforeTransition](uint32_t& bitsAfter) {
		RakNet::BitStream construction;
		component->Serialize(construction, true);
		construction.IgnoreBits(beforeTransition);
		uint8_t state{};
		EXPECT_TRUE(construction.ReadBits(&state, 2));
		bitsAfter = construction.GetNumberOfUnreadBits();
		return state;
	};

	uint32_t normalBits{};
	EXPECT_EQ(transition(normalBits), 0);

	component->SetKeepClientScene(true);
	uint32_t keptBits{};
	EXPECT_EQ(transition(keptBits), 3);
	EXPECT_EQ(keptBits, normalBits);

	component->SetKeepClientScene(false);
	EXPECT_EQ(transition(keptBits), 0);

	player.SetCharacter(nullptr);
}

// Simple physics: live sent motion type Fixed for objects without a motionType (never the invalid 0 DLU sent when the
// level gave none) and no velocity for fixed objects (38,970 Fixed constructions, all without velocity); moving
// platforms were Keyframed with a velocity (1,059 of 1,059).
TEST_F(ReplicaConstructionTest, SimplePhysicsConstructionLikeLive) {
	info.lot = 12266;
	Entity smashable(288300744895979394, info);
	auto* const physics = smashable.AddComponent<SimplePhysicsComponent>(-1);
	physics->SetPosition(NiPoint3(-259.89438f, 77.147575f, 485.42264f));
	NiQuaternion rotation = QuatUtils::IDENTITY;
	rotation.x = 0.51204455f;
	rotation.y = 0.0f;
	rotation.z = 0.0f;
	rotation.w = 0.8589589f;
	physics->SetRotation(rotation);

	RakNet::BitStream construction;
	physics->Serialize(construction, true);
	RakNet::BitStream expected;
	expected.Write0(); // is_climbable
	expected.Write<int32_t>(0); // climbing_property
	expected.Write0(); // velocity_info None
	expected.Write1(); // motion_type Some
	expected.Write<uint32_t>(5); // Fixed
	expected.Write1(); // position_rotation_info
	expected.Write(-259.89438f);
	expected.Write(77.147575f);
	expected.Write(485.42264f);
	expected.Write(0.51204455f); // x, y, z, w
	expected.Write(0.0f);
	expected.Write(0.0f);
	expected.Write(0.8589589f);
	ExpectSameBits(construction, expected);

	info.lot = 11950;
	Entity platform(288300744895900003, info);
	auto* const platformPhysics = platform.AddComponent<SimplePhysicsComponent>(-1);
	platform.AddComponent<MovingPlatformComponent>(-1, "");
	EXPECT_EQ(platformPhysics->GetPhysicsMotionState(), SimplePhysicsComponent::MOTION_TYPE_KEYFRAMED);
	RakNet::BitStream platformConstruction;
	platformPhysics->Serialize(platformConstruction, true);
	platformConstruction.IgnoreBits(1 + 32);
	bool hasVelocity{};
	EXPECT_TRUE(platformConstruction.Read(hasVelocity));
	EXPECT_TRUE(hasVelocity);

	// A property model without behaviors is keyframed
	info.lot = 14;
	Entity model(288300744895908946, info);
	auto* const modelPhysics = model.AddComponent<SimplePhysicsComponent>(-1);
	model.AddComponent<ModelComponent>(-1)->LoadBehaviors();
	EXPECT_EQ(modelPhysics->GetPhysicsMotionState(), SimplePhysicsComponent::MOTION_TYPE_KEYFRAMED);

	// A level-set motion type wins
	info.settings.Insert<uint32_t>(u"motionType", 1);
	Entity dynamic(288300744895900004, info);
	EXPECT_EQ(dynamic.AddComponent<SimplePhysicsComponent>(-1)->GetPhysicsMotionState(), SimplePhysicsComponent::MOTION_TYPE_DYNAMIC);
	info.settings.values.clear();
}

// The client reads a moving platform's subcomponents while a 1 bit comes before one (as live wrote them): the list
// ends with a 0 bit, which DLU left out, so the client took the next component's first bit for it
TEST_F(ReplicaConstructionTest, MovingPlatformEndsItsSubcomponentList) {
	info.lot = 2349;
	Entity platform(288300744895900005, info);
	auto* const component = platform.AddComponent<MovingPlatformComponent>(-1, "");
	component->SetSerialized(true);
	RakNet::BitStream stream;
	component->Serialize(stream, true);

	bool bit{};
	ASSERT_TRUE(stream.Read(bit));
	EXPECT_TRUE(bit); // has subcomponents
	ASSERT_TRUE(stream.Read(bit));
	EXPECT_TRUE(bit); // the path is written with the construction
	ASSERT_TRUE(stream.Read(bit));
	EXPECT_FALSE(bit); // no path
	ASSERT_TRUE(stream.Read(bit));
	EXPECT_TRUE(bit); // a subcomponent follows
	uint32_t type{};
	ASSERT_TRUE(stream.Read(type));
	EXPECT_EQ(type, 4u); // mover
	// The mover: a 1 bit, state, desired waypoint, 2 flags, percent, position, current and next waypoint, 2 times
	stream.IgnoreBits(1 + 32 + 32 + 2 + 32 + 96 + 32 + 32 + 32 + 32);
	ASSERT_TRUE(stream.Read(bit));
	EXPECT_FALSE(bit); // no more subcomponents
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

// Live replicated the DestructibleComponent factionList, so a list of -1 as [-1] (12,829 constructions: vendors,
// quickbuilds, bouncers; DLU dropped it and sent []), and a row with no faction but factionList 6 as [6].
TEST_F(ReplicaConstructionTest, TemplateFactionMinusOneIsReplicated) {
	CDClientDatabase::Connect(":memory:");
	CDClientDatabase::ExecuteDML("CREATE TABLE Factions (faction INTEGER, enemyList TEXT);");
	CDClientDatabase::ExecuteDML("INSERT INTO Factions VALUES (4, '1'), (6, '');");

	Entity vendor(15, info);
	auto* const destroyable = vendor.AddComponent<DestroyableComponent>(-1);
	destroyable->AddTemplateFactions("-1");
	EXPECT_EQ(destroyable->GetFactionIDs(), std::vector<int32_t>{ -1 });

	Entity enemy(16, info);
	auto* const enemyDestroyable = enemy.AddComponent<DestroyableComponent>(-1);
	enemyDestroyable->AddTemplateFactions("4");
	EXPECT_EQ(enemyDestroyable->GetFactionIDs(), std::vector<int32_t>{ 4 });
	EXPECT_EQ(enemyDestroyable->GetEnemyFactionsIDs(), std::vector<int32_t>{ 1 });

	Entity smashable(17, info);
	auto* const smashableDestroyable = smashable.AddComponent<DestroyableComponent>(-1);
	smashableDestroyable->AddTemplateFactions("6");
	EXPECT_EQ(smashableDestroyable->GetFactionIDs(), std::vector<int32_t>{ 6 });
}

// An object with an item component (and no model component): live wrote the UGC info on every construction (99 of 99
// constructions of LOT 14535): ug_id 0, NoStatus, no description. DLU wrote none.
TEST_F(ReplicaConstructionTest, ItemConstructionWritesEmptyUgcInfo) {
	Entity item(15, info);
	auto* const component = item.AddComponent<ItemComponent>(-1);
	RakNet::BitStream construction;
	component->Serialize(construction, true);
	RakNet::BitStream expected;
	expected.Write1();
	expected.Write<LWOOBJID>(0);
	expected.Write<uint32_t>(0);
	expected.Write0();
	ExpectSameBits(construction, expected);

	RakNet::BitStream serialization;
	component->Serialize(serialization, false);
	EXPECT_EQ(serialization.GetNumberOfBitsUsed(), 1u);
}

class InventoryConstructionTest : public GameDependenciesTest {
protected:
	// The items of the live sample below (CDClient 1.10.64 values)
	static constexpr LOT HOOD = 2642; // hair, bind on pickup, proxy 10482
	static constexpr LOT HOOD_PROXY = 10482; // clavicle, bind on equip
	static constexpr LOT NPC_HELMET = 8520; // hair, item type 22 (a model), bind on equip
	static constexpr LOT NPC_KNIFE = 16682; // special_r, bind on equip
	static constexpr LOT PLAIN_SHIRT = 4000; // chest, bound neither way

	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:");
		CDClientDatabase::ExecuteDML("CREATE TABLE ItemSets (setID INTEGER, itemIDs TEXT);");
		RegisterItem(HOOD, "hair", 2, true, false, std::to_string(HOOD_PROXY));
		RegisterItem(HOOD_PROXY, "clavicle", 4, false, true);
		RegisterItem(NPC_HELMET, "hair", 22, false, true);
		RegisterItem(NPC_KNIFE, "special_r", 6, false, true);
		RegisterItem(PLAIN_SHIRT, "chest", 15, false, false);
	}

	void TearDown() override { TearDownDependencies(); }

	static void RegisterItem(const LOT lot, const std::string& equipLocation, const int32_t itemType, const bool isBOP, const bool isBOE, const std::string& subItems = "") {
		const auto componentID = static_cast<uint32_t>(97000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.equipLocation = equipLocation;
		component.itemType = itemType;
		component.isBOP = isBOP;
		component.isBOE = isBOE;
		component.subItems = subItems;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	static void RegisterNpc(const LOT npc, const uint32_t componentID, const std::vector<LOT>& items) {
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(npc), 0); // the LOT's components are cached
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::INVENTORY) << 32 | static_cast<uint64_t>(npc), componentID);
		auto& table = CDClientManager::GetEntriesMutable<CDInventoryComponentTable>();
		for (const auto item : items) table.push_back({ componentID, static_cast<uint32_t>(item), 1, true });
	}

	// One EquippedItemInfo the way lu_packets reads it
	static void WriteItem(RakNet::BitStream& out, const LWOOBJID id, const LOT lot, const uint16_t slot, const uint32_t inventoryType, const bool bound) {
		out.Write(id);
		out.Write(lot);
		out.Write0(); // subkey
		out.Write1(); // count
		out.Write<uint32_t>(1);
		out.Write(slot != 0);
		if (slot != 0) out.Write(slot);
		out.Write(inventoryType != 0);
		if (inventoryType != 0) out.Write(inventoryType);
		out.Write0(); // extra_info
		out.Write(bound);
	}

	static std::vector<EquippedItem> Equipped(InventoryComponent& inventory) {
		std::vector<EquippedItem> items;
		for (const auto& item : inventory.GetEquippedItems() | std::views::values) items.push_back(item);
		return items;
	}
};

// Live, NPC LOT 7426 (FV Numb Chuck; capture 60a58346ece1 idx 58): the helmet, a model, is in the MODELS inventory, so
// it has inventory_type Model and slot 0 like the knife in ITEMS; both bind on equip, so both are bound.
// Live, NPC LOT 13790 (NJ Cole; capture 8846569a7d52 idx 10614): the hood's proxy is TempEquip, slot 0, bound.
// Every live construction ended with an empty equipped_model_transforms list.
TEST_F(InventoryConstructionTest, NpcItemsLikeLive) {
	RegisterNpc(7426, 263, { NPC_HELMET, NPC_KNIFE });
	info.lot = 7426;
	Entity numbChuck(288300744895900001, info);
	auto* const inventory = numbChuck.AddComponent<InventoryComponent>(-1);
	auto items = Equipped(*inventory);
	ASSERT_EQ(items.size(), 2u);

	RakNet::BitStream construction;
	inventory->Serialize(construction, true);
	RakNet::BitStream expected;
	expected.Write1();
	expected.Write<uint32_t>(2);
	WriteItem(expected, items[0].id, NPC_HELMET, 0, 5 /* Model */, true); // "hair" sorts before "special_r"
	WriteItem(expected, items[1].id, NPC_KNIFE, 0, 0, true);
	expected.Write1(); // equipped_model_transforms Some([])
	expected.Write<uint32_t>(0);
	ExpectSameBits(construction, expected);

	RegisterNpc(13790, 516, { HOOD });
	info.lot = 13790;
	Entity cole(288300744895900002, info);
	auto* const coleInventory = cole.AddComponent<InventoryComponent>(-1);
	items = Equipped(*coleInventory);
	ASSERT_EQ(items.size(), 2u);

	RakNet::BitStream coleConstruction;
	coleInventory->Serialize(coleConstruction, true);
	RakNet::BitStream coleExpected;
	coleExpected.Write1();
	coleExpected.Write<uint32_t>(2);
	WriteItem(coleExpected, items[0].id, HOOD_PROXY, 0, 4 /* TempEquip */, true); // "clavicle" before "hair"
	WriteItem(coleExpected, items[1].id, HOOD, 0, 0, true);
	coleExpected.Write1();
	coleExpected.Write<uint32_t>(0);
	ExpectSameBits(coleConstruction, coleExpected);

	// The equipment put on before the construction still goes to the clients already watching, once
	RakNet::BitStream pending;
	coleInventory->Serialize(pending, false);
	EXPECT_GT(pending.GetNumberOfBitsUsed(), 2u);

	// A serialization with nothing changed writes neither list
	RakNet::BitStream serialization;
	coleInventory->Serialize(serialization, false);
	EXPECT_EQ(serialization.GetNumberOfBitsUsed(), 2u);
}

// A player's items: is_bound is the item's bound state (live: bind-on-pickup and equipped bind-on-equip items true,
// items that bind neither way false, 2,990 of 49,534), and proxies are TempEquip.
TEST_F(InventoryConstructionTest, PlayerItemsLikeLive) {
	info.lot = 1;
	CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
	Entity player(0x1000000000000001LL, info);
	auto* const inventory = player.AddComponent<InventoryComponent>(-1);
	Character character(1, nullptr);
	player.SetCharacter(&character);
	player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS)->InitializeStatisticsFromString("");

	auto* const bag = inventory->GetInventory(eInventoryType::ITEMS);
	// A bind-on-pickup item is bound from the moment it is picked up
	auto* const hood = new Item(0x1000000000007000LL, HOOD, bag, 0, 1, true, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	auto* const shirt = new Item(0x1000000000007001LL, PLAIN_SHIRT, bag, 1, 1, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	inventory->EquipItem(hood);
	inventory->EquipItem(shirt);

	LWOOBJID proxyID = LWOOBJID_EMPTY;
	for (const auto& item : Equipped(*inventory)) if (item.lot == HOOD_PROXY) proxyID = item.id;
	ASSERT_NE(proxyID, LWOOBJID_EMPTY);

	RakNet::BitStream construction;
	inventory->Serialize(construction, true);
	RakNet::BitStream expected;
	expected.Write1();
	expected.Write<uint32_t>(3);
	WriteItem(expected, shirt->GetId(), PLAIN_SHIRT, 1, 0, false); // "chest", "clavicle", "hair"
	WriteItem(expected, proxyID, HOOD_PROXY, 0, 4 /* TempEquip */, true);
	WriteItem(expected, hood->GetId(), HOOD, 0, 0, true);
	expected.Write1();
	expected.Write<uint32_t>(0);
	ExpectSameBits(construction, expected);

	player.SetCharacter(nullptr);
}
