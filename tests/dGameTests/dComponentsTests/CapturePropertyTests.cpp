// A property as a packet capture saw it (CaptureProperty): a synthetic capture made from the server's own writers (a
// brick built model and its replica packets, and the property's game messages as the server and client send them),
// read back into the models' timeline, the property's info and the events the capture page lists.
#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "BrickByBrick.h"
#include "CaptureBundle.h"
#include "CaptureProperty.h"
#include "CaptureTools.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "GameMessageDecoder.h"
#include "MessageIdentifiers.h"
#include "MessageType/Client.h"
#include "MessageType/World.h"
#include "ModelComponent.h"
#include "PacketDecoder.h"
#include "PropertyMessages.h"
#include "ReplicaDecoder.h"
#include "ServiceType.h"
#include "SimplePhysicsComponent.h"
#include "eReplicaComponentType.h"
#include "eReplicaPacketType.h"

namespace {
	using json = nlohmann::json;
	using enum eReplicaComponentType;

	constexpr LWOOBJID PLAYER = 1152921506064087003;
	constexpr LWOOBJID FIRST = 288300744895900001, SECOND = 288300744895900002, THIRD = 288300744895900003;

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

	std::string Destruction(uint16_t network) {
		RakNet::BitStream stream;
		stream.Write<uint8_t>(ID_REPLICA_MANAGER_DESTRUCTION);
		stream.Write(network);
		return Bytes(stream);
	}

	// A game message as the server sends it (CLIENT GAME_MSG) or the client does (WORLD GAME_MSG)
	std::string GameMessage(bool toServer, LWOOBJID object, const GameMessages::NetGameMsg& message) {
		RakNet::BitStream stream;
		if (toServer) LUBitStream(ServiceType::WORLD, MessageType::World::GAME_MSG).WriteHeader(stream);
		else LUBitStream(ServiceType::CLIENT, MessageType::Client::GAME_MSG).WriteHeader(stream);
		stream.Write(object);
		stream.Write(message.msgId);
		message.Serialize(stream);
		return Bytes(stream);
	}

	struct Capture {
		std::vector<CaptureBundle::Record> records;

		// One world packet, a second after the one before
		void Add(bool fromClient, std::string bytes) {
			CaptureBundle::Record record;
			record.header.seq = static_cast<uint32_t>(records.size());
			record.header.timeUs = 1000000 * static_cast<int64_t>(records.size() + 1);
			record.header.source = static_cast<uint8_t>(eCaptureSource::WORLD);
			record.header.direction = static_cast<uint8_t>(fromClient ? ePacketDirection::RECEIVED : ePacketDirection::SENT);
			record.header.zoneId = 1150;
			record.header.instanceId = 7;
			record.header.cloneId = 42;
			record.header.characterId = PLAYER;
			record.header.bits = static_cast<uint32_t>(bytes.size() * 8);
			record.bytes = std::move(bytes);
			records.push_back(std::move(record));
		}
	};

	GameMessages::DownloadPropertyData PropertyData(const std::u16string& name, uint64_t reputation) {
		GameMessages::DownloadPropertyData data;
		data.propertyId = 1234;
		data.mapId = 1150;
		data.name = name;
		data.description = u"A garden";
		data.ownerName = u"Builder";
		data.ownerId = PLAYER;
		data.reputation = reputation;
		data.accessType = 2;
		data.moderationStatus = 1;
		return data;
	}
}

class CapturePropertyTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		PacketDecoder::SetGameMessageDecoder([](MessageType::Game id, bool toServer, RakNet::BitStream& payload) {
			return GameMessageDecoder::Decode(id, toServer, payload);
		});
	}
	void TearDown() override { TearDownDependencies(); }

	std::unique_ptr<Entity> MakeModel(LWOOBJID id, const NiPoint3& position, LWOOBJID blueprint) {
		info.lot = BrickByBrick::MODEL_OBJECT_LOT;
		info.settings = {};
		info.settings.Insert<LWOOBJID>(u"blueprintid", blueprint);
		auto model = std::make_unique<Entity>(id, info);
		model->AddComponent<SimplePhysicsComponent>(-1)->SetPosition(position);
		model->AddComponent<ModelComponent>(-1)->LoadBehaviors();
		model->AddComponent<DestroyableComponent>(-1)->SetIsSmashable(true);
		return model;
	}
};

TEST_F(CapturePropertyTest, ModelsInfoAndEventsOverTime) {
	Capture capture;
	const NiPoint3 a(1.0f, 2.0f, 3.0f), b(10.0f, 2.0f, -4.0f), c(5.0f, 2.0f, 5.0f);
	auto first = MakeModel(FIRST, a, 5551);
	auto second = MakeModel(SECOND, b, 5552);

	capture.Add(false, GameMessage(false, PLAYER, PropertyData(u"Old name", 10)));   // 0: the property as it loads
	capture.Add(false, Construction(*first, 20));                                    // 1: a model already there
	GameMessages::PlaceModelResponse placed;
	placed.position = b;
	placed.response = BrickByBrick::PLACE_MODEL_PLACED;
	capture.Add(false, GameMessage(false, PLAYER, placed));                          // 2: the server placed one...
	capture.Add(false, Construction(*second, 21));                                   // 3: ...and made it
	first->GetComponent<SimplePhysicsComponent>()->SetPosition(c);
	capture.Add(false, Serialization(*first, 20));                                   // 4: the first one moves
	GameMessages::PlayBehaviorSound sound;
	sound.soundID = 3;
	capture.Add(false, GameMessage(false, FIRST, sound));                            // 5: its behavior plays a sound
	GameMessages::DeleteModelFromClient pickUp;
	pickUp.modelID = SECOND;
	pickUp.reason = static_cast<int32_t>(BrickByBrick::eDeleteReason::PICKING_MODEL_UP);
	capture.Add(true, GameMessage(true, PLAYER, pickUp));                            // 6: the player picks the second up
	capture.Add(false, Destruction(21));                                             // 7: and it goes
	capture.Add(false, GameMessage(false, PLAYER, PropertyData(u"New name", 25)));   // 8: renamed
	auto elsewhere = PropertyData(u"Another property", 5);
	elsewhere.mapId = 1250;
	capture.Add(false, GameMessage(false, PLAYER, elsewhere));                       // 9: the player's property on another map
	capture.Add(false, GameMessage(false, PLAYER, PropertyData(u"New name", 25)));   // 10: the same data again
	// This server makes the model before it answers the placement
	const NiPoint3 d(-8.8f, 455.04f, 126.4f);
	auto third = MakeModel(THIRD, d, 5553);
	capture.Add(false, Construction(*third, 22));                                    // 11: made...
	placed.position = d;
	capture.Add(false, GameMessage(false, PLAYER, placed));                          // 12: ...then placed

	const ReplicaDecoder::ComponentTable table{ { BrickByBrick::MODEL_OBJECT_LOT, {} } };
	ReplicaDecoder::Session session(table);
	std::vector<std::optional<json>> replica(capture.records.size());
	for (size_t i = 0; i < capture.records.size(); i++) replica[i] = session.Decode(capture.records[i].bytes, CaptureTools::ReplicaConnection(capture.records[i].header));
	const auto out = CaptureProperty::Build(capture.records, 0, [&](size_t i) { return replica[i] ? &*replica[i] : nullptr; });

	ASSERT_EQ(out["worlds"].size(), 1u) << out.dump();
	const auto& world = out["worlds"][0];
	EXPECT_EQ(world["zone"], 1150);
	EXPECT_EQ(world["instance"], 7);
	EXPECT_EQ(world["clone"], 42);

	// The property as each DownloadPropertyData said it, leaving out the one about another map
	ASSERT_EQ(world["info"].size(), 2u);
	EXPECT_EQ(world["info"][0]["name"], "Old name");
	EXPECT_EQ(world["info"][0]["ownerName"], "Builder");
	EXPECT_EQ(world["info"][0]["reputation"], "10");
	EXPECT_EQ(world["info"][0]["i"], 0);
	EXPECT_EQ(world["info"][1]["name"], "New name");
	EXPECT_EQ(world["info"][1]["reputation"], "25");

	// The models: where each stood and when
	ASSERT_EQ(world["models"].size(), 3u);
	const auto& m1 = world["models"][0];
	EXPECT_EQ(m1["object"], std::to_string(FIRST));
	EXPECT_EQ(m1["lot"], BrickByBrick::MODEL_OBJECT_LOT);
	EXPECT_EQ(m1["ugcId"], "5551");
	ASSERT_EQ(m1["spans"].size(), 2u) << m1.dump();
	EXPECT_EQ(m1["spans"][0]["from"], 2.0);
	EXPECT_EQ(m1["spans"][0]["to"], 5.0);
	EXPECT_EQ(m1["spans"][0]["position"], json::array({ 1.0f, 2.0f, 3.0f }));
	EXPECT_EQ(m1["spans"][1]["from"], 5.0);
	EXPECT_TRUE(m1["spans"][1]["to"].is_null());
	EXPECT_EQ(m1["spans"][1]["position"], json::array({ 5.0f, 2.0f, 5.0f }));
	const auto& m2 = world["models"][1];
	EXPECT_EQ(m2["ugcId"], "5552");
	ASSERT_EQ(m2["spans"].size(), 1u);
	EXPECT_EQ(m2["spans"][0]["from"], 4.0);
	EXPECT_EQ(m2["spans"][0]["to"], 8.0);

	// What the capture page lists: placed (the server's answer, with the model it made), moved, the behavior's
	// message and removed (with the reason the client gave)
	std::vector<std::string> kinds;
	for (const auto& event : world["events"]) kinds.push_back(event["kind"]);
	EXPECT_EQ(kinds, (std::vector<std::string>{ "placed", "moved", "behavior", "removed", "placed" })) << world["events"].dump();
	EXPECT_EQ(world["events"][0]["i"], 2);
	EXPECT_EQ(world["events"][0]["object"], std::to_string(SECOND));
	EXPECT_EQ(world["events"][0]["made"], 3);
	EXPECT_EQ(world["events"][1]["object"], std::to_string(FIRST));
	EXPECT_EQ(world["events"][2]["message"], "PLAY_BEHAVIOR_SOUND");
	EXPECT_EQ(world["events"][2]["object"], std::to_string(FIRST));
	EXPECT_EQ(world["events"][3]["object"], std::to_string(SECOND));
	EXPECT_EQ(world["events"][3]["reason"], "picked up");
	EXPECT_EQ(world["events"][3]["asked"], 6);
	EXPECT_EQ(world["events"][4]["i"], 12);
	EXPECT_EQ(world["events"][4]["object"], std::to_string(THIRD));
	EXPECT_EQ(world["events"][4]["made"], 11);
}

// A world with no property message and no model isn't a property
TEST_F(CapturePropertyTest, OtherWorldsAreLeftOut) {
	Capture capture;
	GameMessages::PlayBehaviorSound sound;
	capture.Add(false, GameMessage(false, PLAYER, sound));
	const auto out = CaptureProperty::Build(capture.records, 0, [](size_t) { return nullptr; });
	EXPECT_TRUE(out["worlds"].empty()) << out.dump();
}
