// The capture viewer's replica reader (ReplicaDecoder) against the server's own writers: objects are built with real
// components, written with Entity::WriteBaseReplicaData and WriteComponents as a construction or serialization packet,
// and read back. A component whose Serialize changes without its reader fails here.
#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "BuffComponent.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "ItemComponent.h"
#include "LevelProgressionComponent.h"
#include "MessageIdentifiers.h"
#include "ModelComponent.h"
#include "PlayerForcedMovementComponent.h"
#include "PossessorComponent.h"
#include "ReplicaDecoder.h"
#include "SimplePhysicsComponent.h"
#include "SkillComponent.h"
#include "User.h"
#include "eReplicaComponentType.h"
#include "eReplicaPacketType.h"

namespace {
	using json = nlohmann::json;
	using enum eReplicaComponentType;

	std::string Bytes(const RakNet::BitStream& stream) { return std::string(reinterpret_cast<const char*>(stream.GetData()), stream.GetNumberOfBytesUsed()); }

	std::string Construction(Entity& entity, uint16_t network) {
		RakNet::BitStream stream;
		stream.Write<uint8_t>(ID_REPLICA_MANAGER_CONSTRUCTION);
		stream.Write1();
		stream.Write(network);
		entity.WriteBaseReplicaData(stream, eReplicaPacketType::CONSTRUCTION);
		entity.WriteComponents(stream, eReplicaPacketType::CONSTRUCTION);
		return Bytes(stream);
	}

	std::string Serialization(Entity& entity, uint16_t network) {
		RakNet::BitStream stream;
		stream.Write<uint8_t>(ID_REPLICA_MANAGER_SERIALIZE);
		stream.Write(network);
		entity.WriteBaseReplicaData(stream, eReplicaPacketType::SERIALIZATION);
		entity.WriteComponents(stream, eReplicaPacketType::SERIALIZATION);
		return Bytes(stream);
	}

	const json* FieldsOf(const json& fields, const std::string& name) {
		for (const auto& component : fields["components"]) {
			if (component["component"] == name) return &component["fields"];
		}
		return nullptr;
	}

	std::string Names(const json& fields) {
		std::string names;
		for (const auto& component : fields["components"]) names += component["component"].get<std::string>() + " ";
		return names;
	}
}

class ReplicaDecoderTest : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(ReplicaDecoderTest, EnemyConstructionAndSerializationReadBack) {
	info.lot = 4712;
	Entity enemy(288300744895900100, info);
	auto* const physics = enemy.AddComponent<ControllablePhysicsComponent>(-1);
	auto* const destroyable = enemy.AddComponent<DestroyableComponent>(-1);
	destroyable->SetMaxHealth(8.0f);
	destroyable->SetHealth(8);
	physics->SetPosition(NiPoint3(10.0f, 20.0f, 30.0f));

	// A smashable (is_smashable in the zone file): its destroyable isn't in the registry and is written last, which the
	// reader finds by trying that layout when the registry's doesn't read exactly
	const ReplicaDecoder::ComponentTable table{ { 4712, { CONTROLLABLE_PHYSICS } } };
	ReplicaDecoder::Session session(table);
	const auto constructed = session.Decode(Construction(enemy, 7), 1);
	ASSERT_TRUE(constructed);
	EXPECT_FALSE(constructed->contains("(layout did not match)")) << constructed->dump();
	EXPECT_EQ((*constructed)["networkID"], 7);
	EXPECT_EQ((*constructed)["objectID"], "288300744895900100");
	EXPECT_EQ((*constructed)["lot"], 4712);
	EXPECT_EQ(Names(*constructed), "CONTROLLABLE_PHYSICS DESTROYABLE ");
	const auto* health = FieldsOf(*constructed, "DESTROYABLE");
	ASSERT_TRUE(health);
	EXPECT_EQ((*health)["health"], 8);
	EXPECT_EQ((*FieldsOf(*constructed, "CONTROLLABLE_PHYSICS"))["position"], json::array({ 10.0f, 20.0f, 30.0f }));

	// An update: only what changed, read with the components the construction had
	destroyable->SetHealth(3);
	const auto updated = session.Decode(Serialization(enemy, 7), 1);
	ASSERT_TRUE(updated);
	EXPECT_FALSE(updated->contains("(layout did not match)")) << updated->dump();
	EXPECT_EQ((*updated)["lot"], 4712);
	EXPECT_EQ((*FieldsOf(*updated, "DESTROYABLE"))["health"], 3);

	// Another connection never saw the construction
	const auto unknown = session.Decode(Serialization(enemy, 7), 2);
	ASSERT_TRUE(unknown);
	EXPECT_TRUE(unknown->contains("(object not constructed in this capture)"));

	// Destruction: the network ID, and the object it was
	RakNet::BitStream destruction;
	destruction.Write<uint8_t>(ID_REPLICA_MANAGER_DESTRUCTION);
	destruction.Write<uint16_t>(7);
	const auto destroyed = session.Decode(Bytes(destruction), 1);
	ASSERT_TRUE(destroyed);
	EXPECT_EQ((*destroyed)["networkID"], 7);
	EXPECT_EQ((*destroyed)["lot"], 4712);
	EXPECT_TRUE(session.Decode(Serialization(enemy, 7), 1)->contains("(object not constructed in this capture)"));
}

TEST_F(ReplicaDecoderTest, PlayerConstructionReadsTheCharacterParts) {
	User user(UNASSIGNED_SYSTEM_ADDRESS, "tester", "key");
	Character character(1, &user);
	info.lot = 1;
	Entity player(1152921506064087003, info);
	player.SetCharacter(&character);
	character.SetEntity(&player);
	player.AddComponent<ControllablePhysicsComponent>(-1);
	player.AddComponent<PossessorComponent>(-1);
	player.AddComponent<LevelProgressionComponent>(-1);
	player.AddComponent<PlayerForcedMovementComponent>(-1);
	player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS)->InitializeStatisticsFromString("");

	const ReplicaDecoder::ComponentTable table{ { 1, { CONTROLLABLE_PHYSICS, CHARACTER } } };
	ReplicaDecoder::Session session(table);
	const auto constructed = session.Decode(Construction(player, 1), 1);
	ASSERT_TRUE(constructed);
	EXPECT_FALSE(constructed->contains("(layout did not match)")) << constructed->dump();
	EXPECT_EQ(Names(*constructed), "CONTROLLABLE_PHYSICS POSSESSOR LEVEL_PROGRESSION PLAYER_FORCED_MOVEMENT CHARACTER ");
	EXPECT_EQ((*FieldsOf(*constructed, "CHARACTER"))["statistics"].size(), 27u);
}

// A model's destroyable is not in its registry rows: it is written after the other components
TEST_F(ReplicaDecoderTest, ModelWithTheDestroyableTheRegistryDoesNotList) {
	info.lot = 6000;
	Entity model(288300744895900200, info);
	model.AddComponent<SimplePhysicsComponent>(-1);
	model.AddComponent<ModelComponent>(-1)->LoadBehaviors();
	model.AddComponent<ItemComponent>(-1);
	auto* const destroyable = model.AddComponent<DestroyableComponent>(-1);
	destroyable->SetIsSmashable(true);

	const ReplicaDecoder::ComponentTable table{ { 6000, { SIMPLE_PHYSICS, MODEL, ITEM } } };
	ReplicaDecoder::Session session(table);
	const auto constructed = session.Decode(Construction(model, 3), 1);
	ASSERT_TRUE(constructed);
	EXPECT_FALSE(constructed->contains("(layout did not match)")) << constructed->dump();
	EXPECT_EQ(Names(*constructed), "SIMPLE_PHYSICS ITEM MODEL DESTROYABLE ");
	EXPECT_EQ((*FieldsOf(*constructed, "DESTROYABLE"))["smashable"], true);
}

// When no layout reads the packet exactly, what read is shown with the rest as bits, never a guess
TEST_F(ReplicaDecoderTest, UnknownLayoutShowsTheRest) {
	info.lot = 4712;
	Entity enemy(288300744895900100, info);
	enemy.AddComponent<ControllablePhysicsComponent>(-1);
	auto packet = Construction(enemy, 9);
	packet += std::string("\x12\x34\x56", 3);

	const ReplicaDecoder::ComponentTable table{ { 4712, { CONTROLLABLE_PHYSICS } } };
	ReplicaDecoder::Session session(table);
	const auto constructed = session.Decode(packet, 1);
	ASSERT_TRUE(constructed);
	EXPECT_TRUE(constructed->contains("(layout did not match)"));
	EXPECT_FALSE((*constructed)["(rest)"].get<std::string>().empty());
}

// A live construction (network ID 11, LOT 13006, no components, parent/child info present but empty): the object
// header reads the same as the server writes it
TEST_F(ReplicaDecoderTest, LiveConstructionHeader) {
	const unsigned char live[] = { 0x24, 0x85, 0x80, 0x7f, 0x7f, 0xff, 0xff, 0xff, 0x9f, 0x80, 0x00, 0x67, 0x19, 0x00, 0x00, 0x00, 0x5f, 0x99, 0x9d, 0x00, 0x00, 0x80 };
	const ReplicaDecoder::ComponentTable table{ { 13006, {} } };
	ReplicaDecoder::Session session(table);
	const auto constructed = session.Decode(std::string(reinterpret_cast<const char*>(live), sizeof(live)), 1);
	ASSERT_TRUE(constructed);
	EXPECT_FALSE(constructed->contains("(layout did not match)")) << constructed->dump();
	EXPECT_EQ((*constructed)["networkID"], 11);
	EXPECT_EQ((*constructed)["objectID"], "70368744177662");
	EXPECT_EQ((*constructed)["lot"], 13006);
	EXPECT_EQ((*constructed)["timeSinceCreatedMs"], 3814335);
}
