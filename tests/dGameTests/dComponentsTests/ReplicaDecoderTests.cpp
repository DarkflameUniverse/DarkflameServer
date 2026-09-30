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
#include "ZCompression.h"
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
	// The model component writes the item's user-generated-content block itself, where the client reads the item
	model.AddComponent<ModelComponent>(-1)->LoadBehaviors();
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

namespace {
	std::string FromHex(std::string_view hex) {
		std::string bytes;
		for (size_t i = 0; i + 1 < hex.size(); i += 2) bytes += static_cast<char>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16));
		return bytes;
	}

	nlohmann::json DecodeLive(std::string_view hex, LOT lot, std::vector<eReplicaComponentType> registry) {
		const ReplicaDecoder::ComponentTable table{ { lot, std::move(registry) } };
		ReplicaDecoder::Session session(table);
		const auto decoded = session.Decode(FromHex(hex), 1);
		return decoded ? *decoded : nlohmann::json();
	}
}

// Live constructions (2014 captures): each reads to its last whole byte

// A trigger object (the header's trigger bit set) has a trigger component read after all the others: a 1 bit and the
// trigger ID, -1 in every live capture. This trigger volume has no render data: its zone file sets renderDisabled.
TEST_F(ReplicaDecoderTest, LiveTriggerIsReadLast) {
	const auto constructed = DecodeLive("24818c061f8000002000020a0b000000734cf8803de3f04008040000080000000773fe3d00928d12ec38dcef343c926e0c200000000a010e6bd000000002c617e3fbfffffffe", 5652, { RENDER, PHANTOM_PHYSICS });
	EXPECT_FALSE(constructed.contains("(layout did not match)")) << constructed.dump();
	EXPECT_EQ(constructed["trigger"], true);
	EXPECT_EQ(Names(constructed), "PHANTOM_PHYSICS TRIGGER ");
	EXPECT_EQ((*FieldsOf(constructed, "TRIGGER"))["triggerID"], -1);
}

// A phantom physics effect (a gravity scale, 4) with its amount
TEST_F(ReplicaDecoderTest, LivePhantomPhysicsEffect) {
	const auto constructed = DecodeLive("24da8d409f0000002000023f09000000445b780031126020040420000800000004675ecd009b85b16c4cfa9514363a7b4c10000000086db54bf000000005b390e3fc10000003373130f4ffffffff80", 4734, { RENDER, PHANTOM_PHYSICS });
	EXPECT_FALSE(constructed.contains("(layout did not match)")) << constructed.dump();
	const auto* physics = FieldsOf(constructed, "PHANTOM_PHYSICS");
	ASSERT_TRUE(physics);
	EXPECT_EQ((*physics)["effectType"], 4);
	EXPECT_FLOAT_EQ((*physics)["directionalMultiplier"].get<float>(), 0.05f);
}

// A phantom physics effect with its distance range (min and max)
TEST_F(ReplicaDecoderTest, LivePhantomPhysicsDistance) {
	const auto constructed = DecodeLive("24ac038f86800000200002710680000040d88400398e03a00004000008000000009891de442d7ad85434aaaa9c30000000000000000000000000000803fc000000000001210a0001007e0000908522b93642588a88c107b330c1ffffffff80", 3554, { RENDER, PHANTOM_PHYSICS });
	EXPECT_FALSE(constructed.contains("(layout did not match)")) << constructed.dump();
	const auto* physics = FieldsOf(constructed, "PHANTOM_PHYSICS");
	ASSERT_TRUE(physics);
	EXPECT_TRUE(physics->contains("minDistance"));
	EXPECT_TRUE(physics->contains("maxDistance"));
}

// A moving platform: its mover subcomponent, then the 0 bit that ends the subcomponent list
TEST_F(ReplicaDecoderTest, LiveMovingPlatform) {
	const auto constructed = DecodeLive("248d800e0000000020000216848000006f3786801da2c3b0000400000800000005d45238fc80000000080000000000000000000000000000000000000000000000041000000346f43d874d0ca28689b34f82177a657ee056e67a8f0a6c7f7f99a679a0800000109000000ffffffff0000000000000000000000000000000000000000000000000000000000000000000000000", 2349, { RENDER, SIMPLE_PHYSICS, MOVING_PLATFORM, PLATFORM_BOUNDARY });
	EXPECT_FALSE(constructed.contains("(layout did not match)")) << constructed.dump();
	const auto* platform = FieldsOf(constructed, "MOVING_PLATFORM");
	ASSERT_TRUE(platform);
	ASSERT_EQ((*platform)["subcomponents"].size(), 1u);
	EXPECT_EQ((*platform)["subcomponents"][0]["type"], 4);
}

// An enemy mid-attack: the skill it is casting with its running behaviors
TEST_F(ReplicaDecoderTest, LiveSkillsInProgress) {
	const auto constructed = DecodeLive("24dff38ba48300002000021612000000200a80001c9f33a0000000000808000000800000000414000002a3720b896d24b48794199a820000000000000000000000000001007e4000000000000000000000000000000000000000000000000000000000000000000000002020000000001007e00000000000000000000000000000000000000000000200fc0000000000000000040000002c000000480800000008000001f81800000000000000000000080000001000000471e0000465c8000090000000ba48300002000020ba48300002000020000000000000000000000000000000000000000000000000", 9260, { RENDER, SIMPLE_PHYSICS, SCRIPT, DESTROYABLE, SKILL });
	EXPECT_FALSE(constructed.contains("(layout did not match)")) << constructed.dump();
	const auto* skill = FieldsOf(constructed, "SKILL");
	ASSERT_TRUE(skill);
	ASSERT_FALSE((*skill)["skillsInProgress"].empty());
	EXPECT_FALSE((*skill)["skillsInProgress"][0]["behaviors"].empty());
}

// An object the zone file sets markedAsPhantom on: phantom physics where its LOT lists simple physics
TEST_F(ReplicaDecoderTest, LiveMarkedAsPhantom) {
	const auto constructed = DecodeLive("24a302e403000000200002390e0000002cdd22009a506004040420000800000005e050b8f89da600ec2e412b3439e8626c300000000defb533f00000000e2850f3f8800000000000000000000000000000000000000000000000000000000000000000000000404000000000200fc00000000000000000000000000000000000000000000401f8000000000000000008000001b80000008000000000", 7282, { RENDER, SIMPLE_PHYSICS, DESTROYABLE, SKILL });
	EXPECT_FALSE(constructed.contains("(layout did not match)")) << constructed.dump();
	EXPECT_EQ(Names(constructed), "PHANTOM_PHYSICS BUFF DESTROYABLE SKILL RENDER ");
}

// Compressed LDF (as live sent item and script settings): u32 size, a 1 byte, u32 uncompressed and compressed sizes,
// then zlib data holding the entries, shown inflated
TEST_F(ReplicaDecoderTest, CompressedLdfIsInflated) {
	RakNet::BitStream entries;
	entries.Write<int32_t>(1);
	const std::u16string key = u"name";
	entries.Write<uint8_t>(key.size() * 2);
	for (const auto c : key) entries.Write<uint16_t>(c);
	entries.Write<uint8_t>(1); // i32
	entries.Write<int32_t>(42);
	std::vector<uint8_t> compressed(ZCompression::GetMaxCompressedLength(entries.GetNumberOfBytesUsed()));
	const auto compressedSize = ZCompression::Compress(entries.GetData(), entries.GetNumberOfBytesUsed(), compressed.data(), compressed.size());
	ASSERT_GT(compressedSize, 0);

	RakNet::BitStream packet;
	packet.Write<uint8_t>(ID_REPLICA_MANAGER_CONSTRUCTION);
	packet.Write1();
	packet.Write<uint16_t>(5);
	packet.Write<int64_t>(288300744895900300);
	packet.Write<int32_t>(7000);
	packet.Write<uint8_t>(0); // name
	packet.Write<uint32_t>(0);
	packet.Write1(); // config
	packet.Write<uint32_t>(1 + 4 + 4 + compressedSize);
	packet.Write<uint8_t>(1);
	packet.Write<uint32_t>(entries.GetNumberOfBytesUsed());
	packet.Write<uint32_t>(compressedSize);
	for (int32_t i = 0; i < compressedSize; i++) packet.Write<uint8_t>(compressed[i]);
	for (int i = 0; i < 7; i++) packet.Write0(); // trigger, spawner, spawner node, scale, world state, GM level, parent/child

	const ReplicaDecoder::ComponentTable table{ { 7000, {} } };
	ReplicaDecoder::Session session(table);
	const auto constructed = session.Decode(Bytes(packet), 1);
	ASSERT_TRUE(constructed);
	EXPECT_FALSE(constructed->contains("(layout did not match)")) << constructed->dump();
	ASSERT_TRUE(constructed->contains("config")) << constructed->dump();
	EXPECT_EQ((*constructed)["config"].value("entries", json()), json::array({ "name=1:42" })) << constructed->dump();
}

// Narrow text that isn't UTF-8 is shown byte by byte, so the viewer's JSON always writes
TEST_F(ReplicaDecoderTest, TextThatIsNotUtf8StillWrites) {
	RakNet::BitStream packet;
	packet.Write<uint8_t>(ID_REPLICA_MANAGER_CONSTRUCTION);
	packet.Write1();
	packet.Write<uint16_t>(6);
	packet.Write<int64_t>(288300744895900301);
	packet.Write<int32_t>(7001);
	packet.Write<uint8_t>(0);
	packet.Write<uint32_t>(0);
	packet.Write1(); // config: one narrow string entry with a byte that isn't UTF-8
	RakNet::BitStream entries;
	entries.Write<int32_t>(1);
	entries.Write<uint8_t>(2);
	entries.Write<uint16_t>(u'k');
	entries.Write<uint8_t>(13);
	entries.Write<uint32_t>(2);
	entries.Write<uint8_t>(0x10);
	entries.Write<uint8_t>(0xE9);
	packet.Write<uint32_t>(1 + entries.GetNumberOfBytesUsed());
	packet.Write<uint8_t>(0);
	for (uint32_t i = 0; i < entries.GetNumberOfBytesUsed(); i++) packet.Write<uint8_t>(entries.GetData()[i]);
	for (int i = 0; i < 7; i++) packet.Write0();

	const ReplicaDecoder::ComponentTable table{ { 7001, {} } };
	ReplicaDecoder::Session session(table);
	const auto constructed = session.Decode(Bytes(packet), 1);
	ASSERT_TRUE(constructed);
	EXPECT_NO_THROW(constructed->dump());
	EXPECT_EQ((*constructed)["config"][0], "k=13:\x10\xC3\xA9");
}

// A model outside a property (a model reward in the world): the client makes the plain model component, which reads
// only the model's block, no behaviors; not in an inventory, it is smashable, with its destroyable last
TEST_F(ReplicaDecoderTest, LiveModelOutsideAProperty) {
	const auto constructed = DecodeLive("248a802380000000200002458b800000375d87004f800000004f0000000d4000001e2718d898180432997219d2185219721842186a190a002c8620708318a94331501d8460cbd737a4f061606cd6e700430005b78231d2f2f3b0000400000800000000800000000414000002546af7863532ba87960375840000000000000000000000000001007f8b1700000000000000000000400000000546af7863532ba87960375840001007e000000000000000000000000000000000", 6027, { RENDER, SIMPLE_PHYSICS, ITEM, MODEL });
	EXPECT_FALSE(constructed.contains("(layout did not match)")) << constructed.dump();
	EXPECT_EQ(Names(constructed), "SIMPLE_PHYSICS ITEM MODEL RENDER DESTROYABLE ");
	EXPECT_FALSE(FieldsOf(constructed, "MODEL")->contains("behaviors"));
}

// DLU's models: a property model (propertyObjectID in its settings) writes its behaviors, which the client reads with
// the mutable model component; any other writes only the model's block
TEST_F(ReplicaDecoderTest, ModelBehaviorsOnlyOnPropertyModels) {
	const ReplicaDecoder::ComponentTable table{ { 6001, { SIMPLE_PHYSICS, ITEM, MODEL } } };
	for (const bool onProperty : { false, true }) {
		info.lot = 6001;
		info.settings.values.clear();
		if (onProperty) info.settings.Insert<bool>(u"propertyObjectID", true);
		Entity model(288300744895900210, info);
		model.AddComponent<SimplePhysicsComponent>(-1);
		model.AddComponent<ModelComponent>(-1)->LoadBehaviors();
		ReplicaDecoder::Session session(table);
		const auto constructed = session.Decode(Construction(model, 4), 1);
		ASSERT_TRUE(constructed);
		EXPECT_FALSE(constructed->contains("(layout did not match)")) << constructed->dump();
		const auto* fields = FieldsOf(*constructed, onProperty ? "MUTABLE_MODEL_BEHAVIORS" : "MODEL");
		ASSERT_TRUE(fields) << Names(*constructed);
		EXPECT_EQ(fields->contains("behaviors"), onProperty);
	}
	info.settings.values.clear();
}
