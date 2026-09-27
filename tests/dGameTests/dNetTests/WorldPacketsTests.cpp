#include "ChatPackets.h"
#include "ClientPackets.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"
#include "WorldPackets.h"
#include "Legacy/WorldPacketsLegacy.h"

#include "eCharacterCreationResponse.h"
#include "eGameMasterLevel.h"
#include "eRenameResponse.h"
#include "magic_enum.hpp"

#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace PacketTestUtils;

namespace {
	SystemAddress TestAddress() {
		SystemAddress address;
		address.binaryAddress = 0x0100007F;
		address.port = 1234;
		return address;
	}

	void ExpectSameSend(const std::function<void(const SystemAddress&)>& legacy, const LUBitStream& packet) {
		const auto sysAddr = TestAddress();
		const auto expected = Capture([&] { legacy(sysAddr); });
		const auto actual = Capture([&] { packet.Send(sysAddr); });
		ASSERT_EQ(expected.size(), 1);
		ASSERT_EQ(actual.size(), 1);
		EXPECT_PACKET_EQ(FromCapture(expected[0]), FromCapture(actual[0]));
		EXPECT_EQ(expected[0].sysAddr, actual[0].sysAddr);
		EXPECT_EQ(expected[0].broadcast, actual[0].broadcast);
	}

	template<typename T>
	T RoundTrip(const T& packet) {
		RakNet::BitStream first;
		packet.WritePacket(first);
		T copy;
		EXPECT_TRUE(copy.ReadHeader(first));
		EXPECT_TRUE(copy.Deserialize(first));
		EXPECT_EQ(first.GetNumberOfUnreadBits(), 0);
		RakNet::BitStream second;
		copy.WritePacket(second);
		EXPECT_PACKET_EQ(FromBitStream(first), FromBitStream(second));
		return copy;
	}

	template<typename T>
	void ExpectTruncatedFails(const T& packet, uint32_t keepAtLeast = 8) {
		RakNet::BitStream full;
		packet.WritePacket(full);
		const auto bytes = full.GetNumberOfBytesUsed();
		for (uint32_t cut = keepAtLeast; cut < bytes; cut++) {
			RakNet::BitStream truncated(full.GetData(), cut, true);
			T copy;
			ASSERT_TRUE(copy.ReadHeader(truncated));
			EXPECT_FALSE(copy.Deserialize(truncated)) << "cut at " << cut << " of " << bytes;
		}
	}

	// A received packet as the servers get it, from bytes written by a struct.
	struct ReceivedPacket {
		RakNet::BitStream bytes;
		Packet packet{};

		explicit ReceivedPacket(const LUBitStream& from) {
			from.WritePacket(bytes);
			packet.systemAddress = TestAddress();
			packet.data = bytes.GetData();
			packet.length = bytes.GetNumberOfBytesUsed();
			packet.bitSize = bytes.GetNumberOfBitsUsed();
		}
	};

	// Reads a struct from a received packet the way the dispatchers do.
	template<typename T>
	bool Read(ReceivedPacket& received, T& out) {
		RakNet::BitStream inStream(received.packet.data, received.packet.length, false);
		if (!out.ReadHeader(inStream)) return false;
		return out.Deserialize(inStream);
	}

	const std::vector<std::string> g_Strings = { "", "a", "Hello World", "127.0.0.1", std::string(32, 'x'), std::string(33, 'y'), std::string(300, 'z') };
	const std::vector<LWOOBJID> g_ObjectIDs = { 0, 1, 1152921508901814000, -1, std::numeric_limits<LWOOBJID>::max() };

	struct FakeCharacter {
		LWOOBJID objectID{};
		std::string name;
		std::string unapprovedName;
		bool nameRejected{};
		uint32_t shirtColor{}, shirtStyle{}, pantsColor{}, hairStyle{}, hairColor{}, leftHand{}, rightHand{}, eyebrows{}, eyes{}, mouth{};
		uint32_t zoneID{}, zoneInstance{}, zoneClone{};
		uint64_t lastLogin{};
		std::vector<LOT> equippedItems;

		LWOOBJID GetObjectID() const { return objectID; }
		const std::string& GetName() const { return name; }
		const std::string& GetUnapprovedName() const { return unapprovedName; }
		bool GetNameRejected() const { return nameRejected; }
		uint32_t GetShirtColor() const { return shirtColor; }
		uint32_t GetShirtStyle() const { return shirtStyle; }
		uint32_t GetPantsColor() const { return pantsColor; }
		uint32_t GetHairStyle() const { return hairStyle; }
		uint32_t GetHairColor() const { return hairColor; }
		uint32_t GetLeftHand() const { return leftHand; }
		uint32_t GetRightHand() const { return rightHand; }
		uint32_t GetEyebrows() const { return eyebrows; }
		uint32_t GetEyes() const { return eyes; }
		uint32_t GetMouth() const { return mouth; }
		uint32_t GetZoneID() const { return zoneID; }
		uint32_t GetZoneInstance() const { return zoneInstance; }
		uint32_t GetZoneClone() const { return zoneClone; }
		uint64_t GetLastLogin() const { return lastLogin; }
		const std::vector<LOT>& GetEquippedItems() const { return equippedItems; }
	};
}

class WorldPacketsTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

// ---- Server -> client ----

TEST_F(WorldPacketsTests, LoadStaticZoneMatchesLegacy) {
	for (const auto& zone : { LWOZONEID(1000, 0, 0), LWOZONEID(1100, 7, 1234), LWOZONEID(0xFFFF, 0xFFFF, 0xFFFFFFFF) }) {
		for (const auto& [x, y, z] : { std::tuple{ 0.0f, 0.0f, 0.0f }, std::tuple{ -1.5f, 200.25f, 1e30f } }) {
			for (const uint32_t checksum : { 0u, 0x20b8087cu, 0xFFFFFFFFu }) {
				ClientPackets::LoadStaticZone packet;
				packet.mapID = zone.GetMapID();
				packet.instanceID = zone.GetInstanceID();
				packet.cloneID = 0; // what the world server sends
				packet.mapChecksum = checksum;
				packet.playerPosition = NiPoint3(x, y, z);
				ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendLoadStaticZone(a, x, y, z, checksum, zone); }, packet);
				EXPECT_EQ(RoundTrip(packet).mapChecksum, checksum);
			}
		}
	}

	ClientPackets::LoadStaticZone packet;
	packet.mapID = 1100;
	packet.instanceID = 2;
	packet.mapChecksum = 0x11223344;
	packet.playerPosition = NiPoint3(1.0f, 2.0f, 3.0f);
	RakNet::BitStream bytes;
	packet.WritePacket(bytes);
	// 0x53 | CLIENT (5) | LOAD_STATIC_ZONE (2) | pad | map | instance | clone | checksum | editor u8 u8 | x y z | instance type
	EXPECT_PACKET_EQ(FromHex("53 05 00 02 00 00 00 00 4c 04 02 00 00 00 00 00 44 33 22 11 00 00 00 00 80 3f 00 00 00 40 00 00 40 40 00 00 00 00"), FromBitStream(bytes));
	ExpectTruncatedFails(packet);
}

TEST_F(WorldPacketsTests, SmallResponsesMatchLegacy) {
	for (const auto response : magic_enum::enum_values<eCharacterCreationResponse>()) {
		ClientPackets::CharacterCreateResponse packet;
		packet.response = response;
		ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendCharacterCreationResponse(a, response); }, packet);
		EXPECT_EQ(RoundTrip(packet).response, response);
	}
	for (const auto response : magic_enum::enum_values<eRenameResponse>()) {
		ClientPackets::CharacterRenameResponse packet;
		packet.response = response;
		ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendCharacterRenameResponse(a, response); }, packet);
		EXPECT_EQ(RoundTrip(packet).response, response);
	}
	for (const bool success : { false, true }) {
		ClientPackets::DeleteCharacterResponse packet;
		packet.success = success;
		ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendCharacterDeleteResponse(a, success); }, packet);
		EXPECT_EQ(RoundTrip(packet).success, success);
	}
	{
		ClientPackets::ServerStates packet;
		ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendServerState(a); }, packet);
		RoundTrip(packet);
	}

	ClientPackets::CharacterCreateResponse created;
	created.response = eCharacterCreationResponse::CUSTOM_NAME_IN_USE;
	RakNet::BitStream bytes;
	created.WritePacket(bytes);
	EXPECT_PACKET_EQ(FromHex("53 05 00 07 00 00 00 00 04"), FromBitStream(bytes));
}

TEST_F(WorldPacketsTests, TransferToWorldMatchesLegacy) {
	for (const auto& ip : g_Strings) {
		for (const uint32_t port : { 0u, 2001u, 0xFFFFu, 0x12345u }) {
			for (const bool mythranShift : { false, true }) {
				ClientPackets::TransferToWorld packet;
				packet.serverIP = LUString(ip);
				packet.serverPort = port;
				packet.mythranShift = mythranShift;
				ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendTransferToWorld(a, ip, port, mythranShift); }, packet);
				const auto copy = RoundTrip(packet);
				EXPECT_EQ(copy.serverPort, static_cast<uint16_t>(port));
				EXPECT_EQ(copy.mythranShift, mythranShift);
			}
		}
	}

	ClientPackets::TransferToWorld packet;
	packet.serverIP = LUString("1.2");
	packet.serverPort = 0x0102;
	packet.mythranShift = true;
	RakNet::BitStream bytes;
	packet.WritePacket(bytes);
	std::string zeros;
	for (int i = 0; i < 30; i++) zeros += " 00";
	EXPECT_PACKET_EQ(FromHex("53 05 00 0e 00 00 00 00 31 2e 32" + zeros + " 02 01 01"), FromBitStream(bytes));
	ExpectTruncatedFails(packet);
}

TEST_F(WorldPacketsTests, MakeGMResponseMatchesLegacy) {
	const auto levels = magic_enum::enum_values<eGameMasterLevel>();
	for (const bool success : { false, true }) {
		for (const auto highest : levels) {
			for (const auto previous : { eGameMasterLevel::CIVILIAN, eGameMasterLevel::OPERATOR }) {
				ClientPackets::MakeGMResponse packet;
				packet.success = success;
				packet.highestLevel = highest;
				packet.previousLevel = previous;
				packet.newLevel = highest;
				ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendGMLevelChange(a, success, highest, previous, highest); }, packet);
				EXPECT_EQ(RoundTrip(packet).highestLevel, highest);
			}
		}
	}

	ClientPackets::MakeGMResponse packet;
	packet.success = true;
	packet.highestLevel = eGameMasterLevel::OPERATOR;
	packet.previousLevel = eGameMasterLevel::CIVILIAN;
	packet.newLevel = eGameMasterLevel::DEVELOPER;
	RakNet::BitStream bytes;
	packet.WritePacket(bytes);
	EXPECT_PACKET_EQ(FromHex("53 05 00 10 00 00 00 00 01 09 00 00 00 08 00"), FromBitStream(bytes));
	ExpectTruncatedFails(packet);
}

TEST_F(WorldPacketsTests, HTTPMonitorInfoAndDebugOutputMatchLegacy) {
	for (int bits = 0; bits < 32; bits++) {
		LegacyWorldPackets::HTTPMonitorInfo info;
		info.port = static_cast<uint16_t>(80 + bits);
		info.openWeb = bits & 1;
		info.supportsSum = bits & 2;
		info.supportsDetail = bits & 4;
		info.supportsWho = bits & 8;
		info.supportsObjects = bits & 16;
		ClientPackets::HTTPMonitorInfoResponse packet;
		packet.port = info.port;
		packet.openWeb = info.openWeb;
		packet.supportsSum = info.supportsSum;
		packet.supportsDetail = info.supportsDetail;
		packet.supportsWho = info.supportsWho;
		packet.supportsObjects = info.supportsObjects;
		ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendHTTPMonitorInfo(a, info); }, packet);
		EXPECT_EQ(RoundTrip(packet).supportsWho, info.supportsWho);
	}

	for (const auto& text : g_Strings) {
		ClientPackets::DebugOutput packet;
		packet.data = text;
		ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendDebugOuput(a, text); }, packet);
		EXPECT_EQ(RoundTrip(packet).data, text);
	}
}

TEST_F(WorldPacketsTests, ChatModerationStringMatchesLegacy) {
	using Segments = std::set<std::pair<uint8_t, uint8_t>>;
	Segments many;
	for (uint8_t i = 0; i < 70; i++) many.emplace(i, static_cast<uint8_t>(i + 1));
	for (const auto& segments : { Segments{}, Segments{ { 0, 4 } }, Segments{ { 2, 3 }, { 10, 255 } }, many }) {
		for (const uint32_t requestID : { 0u, 7u, 255u, 256u }) {
			for (const auto& receiver : g_Strings) {
				for (const bool accepted : { false, true }) {
					ClientPackets::ChatModerationString packet;
					packet.requestAccepted = segments.empty(); // what the world server sends
					packet.requestID = requestID;
					packet.receiver = LUWString(receiver, 42);
					packet.rejectedSegments = segments;
					ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendChatModerationResponse(a, accepted, requestID, receiver, segments); }, packet);
					if (segments.size() <= 64) EXPECT_EQ(RoundTrip(packet).rejectedSegments, segments);
				}
			}
		}
	}
}

TEST_F(WorldPacketsTests, CreateCharacterMatchesLegacy) {
	for (const auto objectID : g_ObjectIDs) {
		for (const auto& xml : { std::string(), std::string("<obj v=\"1\"><char acct=\"1\"/></obj>"), std::string(20000, 'x') }) {
			for (const auto gm : { eGameMasterLevel::CIVILIAN, eGameMasterLevel::OPERATOR }) {
				for (const int64_t reputation : { int64_t{ 0 }, int64_t{ -5 }, std::numeric_limits<int64_t>::max() }) {
					const std::u16string name = u"Some Name";
					const LWOCLONEID cloneID = 1234;
					ClientPackets::CreateCharacter packet;
					packet.objectID = objectID;
					packet.xmlData = xml;
					packet.name = name;
					packet.gmLevel = gm;
					packet.chatMode = static_cast<int32_t>(gm);
					packet.reputation = reputation;
					packet.propertyCloneID = cloneID;
					ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendCreateCharacter(a, reputation, objectID, xml, name, gm, cloneID); }, packet);

					const auto copy = RoundTrip(packet);
					EXPECT_EQ(copy.objectID, objectID);
					EXPECT_EQ(copy.xmlData, xml);
					EXPECT_EQ(copy.name, name);
					EXPECT_EQ(copy.gmLevel, gm);
					EXPECT_EQ(copy.reputation, reputation);
					EXPECT_EQ(copy.propertyCloneID, 1234);
					EXPECT_EQ(copy.templateID, 1);
				}
			}
		}
	}

	ClientPackets::CreateCharacter packet;
	packet.xmlData = "<obj/>";
	ExpectTruncatedFails(packet);
}

TEST_F(WorldPacketsTests, CharacterListMatchesLegacy) {
	FakeCharacter first;
	FakeCharacter second;
	second.objectID = 1152921508901814000;
	second.name = "SomeName";
	second.unapprovedName = std::string(40, 'u');
	second.nameRejected = true;
	second.shirtColor = 1; second.shirtStyle = 2; second.pantsColor = 3; second.hairStyle = 4; second.hairColor = 5;
	second.leftHand = 6; second.rightHand = 7; second.eyebrows = 8; second.eyes = 9; second.mouth = 10;
	second.zoneID = 1100; second.zoneInstance = 0x12345; second.zoneClone = 99;
	second.lastLogin = 1790000000;
	second.equippedItems = { 1, -1, 7000 };

	const std::vector<std::vector<FakeCharacter*>> lists = { {}, { &first }, { &first, &second, &second, &second } };
	for (const auto& list : lists) {
		ClientPackets::CharacterListResponse packet;
		for (const auto* character : list) {
			auto& entry = packet.characters.emplace_back();
			entry.objectID = character->GetObjectID();
			entry.name = LUWString(character->GetName());
			entry.unapprovedName = LUWString(character->GetUnapprovedName());
			entry.nameRejected = character->GetNameRejected();
			entry.shirtColor = character->GetShirtColor();
			entry.shirtStyle = character->GetShirtStyle();
			entry.pantsColor = character->GetPantsColor();
			entry.hairStyle = character->GetHairStyle();
			entry.hairColor = character->GetHairColor();
			entry.leftHand = character->GetLeftHand();
			entry.rightHand = character->GetRightHand();
			entry.eyebrows = character->GetEyebrows();
			entry.eyes = character->GetEyes();
			entry.mouth = character->GetMouth();
			entry.zoneID = character->GetZoneID();
			entry.zoneInstance = character->GetZoneInstance();
			entry.zoneClone = character->GetZoneClone();
			entry.lastLogin = character->GetLastLogin();
			entry.equippedItems = character->GetEquippedItems();
		}
		ExpectSameSend([&](const SystemAddress& a) { LegacyWorldPackets::SendCharacterList(a, list); }, packet);
		const auto copy = RoundTrip(packet);
		ASSERT_EQ(copy.characters.size(), list.size());
		if (list.size() > 1) {
			EXPECT_EQ(copy.characters[1].name.GetAsString(), "SomeName");
			EXPECT_EQ(copy.characters[1].equippedItems, second.equippedItems);
			EXPECT_TRUE(copy.characters[1].nameRejected);
		}
		if (!list.empty()) ExpectTruncatedFails(packet);
	}
}

// ---- Client -> server ----

TEST_F(WorldPacketsTests, ValidationMatchesLegacy) {
	for (const auto& username : g_Strings) {
		for (const auto& checksum : { std::string(), std::string("0123456789abcdef0123456789abcdef") }) {
			WorldPackets::Validation packet;
			packet.username = LUWString(username);
			packet.sessionKey = LUWString(std::string("fdce6d87c07128819c3a13cec99de30a"));
			packet.fdbChecksum = LUString(checksum, 32);
			ReceivedPacket received(packet);
			const auto legacy = LegacyWorldPackets::ReadValidation(&received.packet);
			WorldPackets::Validation read;
			ASSERT_TRUE(Read(received, read));
			EXPECT_EQ(read.username.GetAsString(), legacy.username);
			EXPECT_EQ(read.sessionKey.GetAsString(), legacy.sessionKey);
			EXPECT_EQ(read.fdbChecksum.string, legacy.checksum);
			RoundTrip(packet);
			ExpectTruncatedFails(packet);
		}
	}

	// The optional trailing byte some clients send is ignored
	WorldPackets::Validation packet;
	packet.username = LUWString(std::string("user"));
	RakNet::BitStream bytes;
	packet.WritePacket(bytes);
	bytes.Write<uint8_t>(0);
	WorldPackets::Validation read;
	ASSERT_TRUE(read.ReadHeader(bytes));
	ASSERT_TRUE(read.Deserialize(bytes));
	EXPECT_EQ(read.username.GetAsString(), "user");

	// Golden: header then the username's first characters
	RakNet::BitStream golden;
	packet.WritePacket(golden);
	const auto all = FromBitStream(golden);
	EXPECT_PACKET_EQ(FromHex("53 04 00 01 00 00 00 00 75 00 73 00 65 00 72 00 00 00"), (PacketBytes{ { all.bytes.begin(), all.bytes.begin() + 18 }, 18 * 8 }));
	EXPECT_EQ(all.bytes.size(), 8 + 66 + 66 + 32);
}

TEST_F(WorldPacketsTests, CharacterRequestsMatchLegacy) {
	for (const auto objectID : g_ObjectIDs) {
		WorldPackets::CharacterLoginRequest login;
		login.playerID = objectID;
		ReceivedPacket loginReceived(login);
		WorldPackets::CharacterLoginRequest loginRead;
		ASSERT_TRUE(Read(loginReceived, loginRead));
		EXPECT_EQ(loginRead.playerID, LegacyWorldPackets::ReadLoginRequest(&loginReceived.packet));
		ExpectTruncatedFails(login);

		WorldPackets::CharacterDeleteRequest deletion;
		deletion.objectID = objectID;
		ReceivedPacket deleteReceived(deletion);
		WorldPackets::CharacterDeleteRequest deleteRead;
		ASSERT_TRUE(Read(deleteReceived, deleteRead));
		EXPECT_EQ(deleteRead.objectID, LegacyWorldPackets::ReadDeleteCharacter(&deleteReceived.packet));
		ExpectTruncatedFails(deletion);

		for (const auto& name : g_Strings) {
			WorldPackets::CharacterRenameRequest rename;
			rename.objectID = objectID;
			rename.name = LUWString(name);
			ReceivedPacket renameReceived(rename);
			WorldPackets::CharacterRenameRequest renameRead;
			ASSERT_TRUE(Read(renameReceived, renameRead));
			const auto [legacyID, legacyName] = LegacyWorldPackets::ReadRenameCharacter(&renameReceived.packet);
			EXPECT_EQ(renameRead.objectID, legacyID);
			EXPECT_EQ(renameRead.name.GetAsString(), legacyName);
			RoundTrip(rename);
		}
	}

	for (const auto& name : g_Strings) {
		WorldPackets::CharacterCreateRequest create;
		create.name = LUWString(name);
		create.firstNameIndex = 1; create.middleNameIndex = 2; create.lastNameIndex = 0xFFFFFFFF;
		create.unknown = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
		create.shirtColor = 10; create.shirtStyle = 11; create.pantsColor = 12; create.hairStyle = 13; create.hairColor = 14;
		create.leftHand = 15; create.rightHand = 16; create.eyebrows = 17; create.eyes = 18; create.mouth = 19;
		ReceivedPacket received(create);
		const auto legacy = LegacyWorldPackets::ReadCreateCharacter(&received.packet);
		WorldPackets::CharacterCreateRequest read;
		ASSERT_TRUE(Read(received, read));
		EXPECT_EQ(read.name.GetAsString(), legacy.LUWStringName.GetAsString());
		EXPECT_EQ(read.firstNameIndex, legacy.firstNameIndex);
		EXPECT_EQ(read.middleNameIndex, legacy.middleNameIndex);
		EXPECT_EQ(read.lastNameIndex, legacy.lastNameIndex);
		EXPECT_EQ(read.shirtColor, legacy.shirtColor);
		EXPECT_EQ(read.shirtStyle, legacy.shirtStyle);
		EXPECT_EQ(read.pantsColor, legacy.pantsColor);
		EXPECT_EQ(read.hairStyle, legacy.hairStyle);
		EXPECT_EQ(read.hairColor, legacy.hairColor);
		EXPECT_EQ(read.leftHand, legacy.lh);
		EXPECT_EQ(read.rightHand, legacy.rh);
		EXPECT_EQ(read.eyebrows, legacy.eyebrows);
		EXPECT_EQ(read.eyes, legacy.eyes);
		EXPECT_EQ(read.mouth, legacy.mouth);
		RoundTrip(create);
		ExpectTruncatedFails(create);
	}

	WorldPackets::CharacterListRequest list;
	RakNet::BitStream bytes;
	list.WritePacket(bytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 02 00 00 00 00"), FromBitStream(bytes));
}

TEST_F(WorldPacketsTests, GameMessageMatchesLegacy) {
	for (const auto objectID : g_ObjectIDs) {
		for (const uint32_t payloadBits : { 0u, 1u, 8u, 13u, 800u }) {
			WorldPackets::GameMessage packet;
			packet.objectID = objectID;
			packet.messageID = MessageType::Game::REQUEST_USE;
			for (uint32_t i = 0; i < payloadBits; i++) packet.data.Write(i % 3 == 0);
			ReceivedPacket received(packet);
			const auto legacy = LegacyWorldPackets::ReadGameMessage(&received.packet);
			WorldPackets::GameMessage read;
			ASSERT_TRUE(Read(received, read));
			EXPECT_EQ(read.objectID, legacy.objectID);
			EXPECT_EQ(read.messageID, legacy.messageID);
			// A packet is whole bytes, so the data read is the payload padded to a byte, as before
			EXPECT_EQ(read.data.GetNumberOfBitsUsed(), legacy.dataBits);
			EXPECT_PACKET_EQ((PacketBytes{ legacy.data, legacy.dataBits }), FromBitStream(read.data));
		}
	}

	WorldPackets::GameMessage packet;
	packet.objectID = 0x0102030405060708;
	packet.messageID = static_cast<MessageType::Game>(0x0A0B);
	RakNet::BitStream bytes;
	packet.WritePacket(bytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 05 00 00 00 00 08 07 06 05 04 03 02 01 0b 0a"), FromBitStream(bytes));
	ExpectTruncatedFails(packet);
}

TEST_F(WorldPacketsTests, PositionUpdateMatchesLegacy) {
	for (int flags = 0; flags < 32; flags++) {
		WorldPackets::PositionUpdate packet;
		auto& update = packet.update;
		update.position = NiPoint3(1.0f, -2.0f, 3.5f);
		update.rotation = NiQuaternion(0.5f, 0.1f, 0.2f, 0.3f);
		update.onGround = flags & 1;
		update.onRail = !(flags & 1);
		packet.hasVelocity = flags & 2;
		update.velocity = packet.hasVelocity ? NiPoint3(4.0f, 5.0f, 6.0f) : NiPoint3Constant::ZERO;
		packet.hasAngularVelocity = flags & 4;
		update.angularVelocity = packet.hasAngularVelocity ? NiPoint3(7.0f, 8.0f, 9.0f) : NiPoint3Constant::ZERO;
		packet.hasLocalSpaceInfo = flags & 8;
		packet.hasLinearVelocity = packet.hasLocalSpaceInfo && (flags & 1);
		if (packet.hasLocalSpaceInfo) {
			update.localSpaceInfo.objectId = 1234;
			update.localSpaceInfo.position = NiPoint3(10.0f, 11.0f, 12.0f);
			if (packet.hasLinearVelocity) update.localSpaceInfo.linearVelocity = NiPoint3(13.0f, 14.0f, 15.0f);
		}
		packet.hasRemoteInputInfo = flags & 16;
		if (packet.hasRemoteInputInfo) {
			update.remoteInputInfo.m_RemoteInputX = 0.25f;
			update.remoteInputInfo.m_RemoteInputY = -0.75f;
			update.remoteInputInfo.m_IsPowersliding = true;
			update.remoteInputInfo.m_IsModified = flags & 1;
		}

		ReceivedPacket received(packet);
		const auto legacy = LegacyWorldPackets::HandleClientPositionUpdate(&received.packet);
		WorldPackets::PositionUpdate read;
		ASSERT_TRUE(Read(received, read));
		const auto& got = read.update;
		EXPECT_EQ(got.position, legacy.position);
		EXPECT_EQ(got.rotation, legacy.rotation);
		EXPECT_EQ(got.onGround, legacy.onGround);
		EXPECT_EQ(got.onRail, legacy.onRail);
		EXPECT_EQ(got.velocity, legacy.velocity);
		EXPECT_EQ(got.angularVelocity, legacy.angularVelocity);
		EXPECT_EQ(got.localSpaceInfo.objectId, legacy.localSpaceInfo.objectId);
		EXPECT_EQ(got.localSpaceInfo.position, legacy.localSpaceInfo.position);
		EXPECT_EQ(got.localSpaceInfo.linearVelocity, legacy.localSpaceInfo.linearVelocity);
		auto remote = got.remoteInputInfo;
		EXPECT_TRUE(remote == legacy.remoteInputInfo);
		RoundTrip(packet);
	}

	// Like before, the last two sections may be left out entirely
	WorldPackets::PositionUpdate packet;
	RakNet::BitStream bytes;
	packet.WritePacket(bytes);
	RakNet::BitStream shortened(bytes.GetData(), bytes.GetNumberOfBytesUsed(), true);
	shortened.SetWriteOffset(bytes.GetNumberOfBitsUsed() - 2); // drop the two trailing flags
	WorldPackets::PositionUpdate read;
	ASSERT_TRUE(read.ReadHeader(shortened));
	EXPECT_TRUE(read.Deserialize(shortened));
	EXPECT_FALSE(read.hasLocalSpaceInfo);
	EXPECT_FALSE(read.hasRemoteInputInfo);
}

TEST_F(WorldPacketsTests, StringCheckMatchesLegacy) {
	const std::vector<std::u16string> receivers = { u"", u"Bob", u"[GM]Mythran", u"[GM]", std::u16string(42, u'r') };
	const std::vector<std::u16string> messages = { u"", u"hi", u"hello there", std::u16string(300, u'm') };
	for (const auto& receiver : receivers) {
		for (const auto& message : messages) {
			for (const uint8_t chatLevel : { uint8_t{ 0 }, uint8_t{ 1 } }) {
				WorldPackets::StringCheck packet;
				packet.chatLevel = chatLevel;
				packet.requestID = 0xa2;
				packet.receiver = receiver;
				packet.receiver.resize(42);
				// what clients leave after the name
				if (receiver.size() < 40) packet.receiver[receiver.size() + 1] = u'\x1894';
				packet.message = message;
				ReceivedPacket received(packet);
				const auto legacy = LegacyWorldPackets::HandleChatModerationRequest(&received.packet);
				WorldPackets::StringCheck read;
				ASSERT_TRUE(Read(received, read));
				EXPECT_EQ(read.chatLevel, legacy.chatLevel);
				EXPECT_EQ(read.requestID, legacy.requestID);
				EXPECT_EQ(read.GetNarrowReceiver(), legacy.receiver);
				EXPECT_EQ(read.GetNarrowMessage(), legacy.message);
				RoundTrip(packet);
				ExpectTruncatedFails(packet);
			}
		}
	}
}

TEST_F(WorldPacketsTests, GeneralChatMessageMatchesLegacy) {
	for (const auto& message : { std::u16string(), std::u16string(u"how long are you going to stay there?"), std::u16string(2000, u'x') }) {
		for (const uint8_t channel : { uint8_t{ 0 }, uint8_t{ 4 } }) {
			WorldPackets::GeneralChatMessage packet;
			packet.chatChannel = channel;
			packet.unknown = 0x0900;
			packet.message = message;
			ReceivedPacket received(packet);
			const auto legacy = LegacyWorldPackets::HandleChatMessage(&received.packet);
			WorldPackets::GeneralChatMessage read;
			ASSERT_TRUE(Read(received, read));
			EXPECT_EQ(read.chatChannel, legacy.chatChannel);
			EXPECT_EQ(read.unknown, legacy.unknown);
			EXPECT_EQ(read.message, legacy.message);
			RoundTrip(packet);
		}
	}

	WorldPackets::GeneralChatMessage packet;
	packet.chatChannel = 4;
	packet.message = u"hi";
	RakNet::BitStream bytes;
	packet.WritePacket(bytes);
	// channel | unknown u16 | count 3 (includes the terminator) | "hi" | terminator
	EXPECT_PACKET_EQ(FromHex("53 04 00 0e 00 00 00 00 04 00 00 03 00 00 00 68 00 69 00 00 00"), FromBitStream(bytes));

	// A negative or huge count is dropped
	for (const int32_t count : { -1, static_cast<int32_t>(MAX_MESSAGE_LENGTH) + 1 }) {
		RakNet::BitStream bad;
		LUBitStream(ServiceType::WORLD, MessageType::World::GENERAL_CHAT_MESSAGE).WriteHeader(bad);
		bad.Write<uint8_t>(0);
		bad.Write<uint16_t>(0);
		bad.Write(count);
		WorldPackets::GeneralChatMessage read;
		ASSERT_TRUE(read.ReadHeader(bad));
		EXPECT_FALSE(read.Deserialize(bad));
	}
}

TEST_F(WorldPacketsTests, RoutePacketMatchesLegacy) {
	for (const uint32_t bodySize : { 0u, 1u, 4u, 20u }) {
		for (const uint32_t declaredSize : { 0u, 2u, 11u, 100u, 20001u }) {
			for (const LWOOBJID sender : { LWOOBJID{ 0 }, LWOOBJID{ 1152921508901814000 } }) {
				WorldPackets::RoutePacket packet;
				packet.size = declaredSize;
				packet.routedService = ServiceType::CHAT;
				packet.routedMessageID = 0x10A;
				for (uint32_t i = 0; i < bodySize; i++) packet.routedData.push_back(static_cast<uint8_t>(i + 1));
				ReceivedPacket received(packet);

				RakNet::BitStream legacy;
				const bool legacyForwarded = LegacyWorldPackets::BuildRoutePacket(&received.packet, sender, legacy);

				WorldPackets::RoutePacket read;
				ASSERT_TRUE(Read(received, read));
				EXPECT_EQ(read.routedData, packet.routedData);
				EXPECT_EQ(legacyForwarded, read.size <= 20000); // the world server's size check
				if (!legacyForwarded) continue;

				RakNet::BitStream converted;
				read.ToChat(sender).WritePacket(converted);
				EXPECT_PACKET_EQ(FromBitStream(legacy), FromBitStream(converted));
				RoundTrip(packet);
			}
		}
	}

	WorldPackets::RoutePacket packet;
	ExpectTruncatedFails(packet);
}

TEST_F(WorldPacketsTests, SmallRequestsMatchLegacy) {
	for (const auto type : magic_enum::enum_values<eFunnessTypes>()) {
		for (const float info : { 0.0f, 1.0f, 6.5f, -3.0f }) {
			WorldPackets::HandleFunness packet;
			packet.cheatType = type;
			packet.cheatInfo = info;
			ReceivedPacket received(packet);
			const auto legacy = LegacyWorldPackets::ReadFunness(&received.packet);
			WorldPackets::HandleFunness read;
			ASSERT_TRUE(Read(received, read));
			EXPECT_EQ(read.cheatType, legacy.cheatType);
			EXPECT_EQ(read.cheatInfo, legacy.cheatInfo);
			ExpectTruncatedFails(packet);
		}
	}

	for (const int32_t language : { 0, 1, 2, 3, -1 }) {
		WorldPackets::UIHelpTop5 packet;
		packet.language = language;
		ReceivedPacket received(packet);
		WorldPackets::UIHelpTop5 read;
		ASSERT_TRUE(Read(received, read));
		EXPECT_EQ(read.language, LegacyWorldPackets::SendTop5HelpIssues(&received.packet));
		ExpectTruncatedFails(packet);
	}

	WorldPackets::LevelLoadComplete level;
	level.mapID = 1101;
	level.instanceID = 46671;
	RakNet::BitStream bytes;
	level.WritePacket(bytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 13 00 00 00 00 4d 04 4f b6 00 00 00 00"), FromBitStream(bytes));
	EXPECT_EQ(RoundTrip(level).instanceID, 46671);
	ExpectTruncatedFails(level);

	WorldPackets::MailPacket mail;
	mail.data.Write<uint32_t>(0x01020304);
	RakNet::BitStream mailBytes;
	mail.WritePacket(mailBytes);
	WorldPackets::MailPacket mailRead;
	ASSERT_TRUE(mailRead.ReadHeader(mailBytes));
	ASSERT_TRUE(mailRead.Deserialize(mailBytes));
	EXPECT_PACKET_EQ(FromBitStream(mail.data), FromBitStream(mailRead.data));
}

// The layouts the 1.10.64 client uses without 3D services (UGCUSE3DSERVICES=7:0): it sends 16 bytes after the 0x53
// (SendRequestUGCManifestInfoPacket) and only reads an answer that is exactly 37 bytes after it
// (PacketHandler_MSG_CLIENT_UGC_MANIFEST_RESPONSE: 21 bytes of manifest info after the blueprint and type).
TEST_F(WorldPacketsTests, UgcManifestPacketsMatchTheClient) {
	WorldPackets::RequestUgcManifestInfo request;
	request.blueprintId = 0x0102030405060708;
	request.resourceType = eUgcResourceType::DDS;
	RakNet::BitStream requestBytes;
	request.WritePacket(requestBytes);
	EXPECT_PACKET_EQ(FromHex("53 04 00 1b 00 00 00 00 08 07 06 05 04 03 02 01 03"), FromBitStream(requestBytes));
	EXPECT_EQ(requestBytes.GetNumberOfBytesUsed() - 1, 16);
	EXPECT_EQ(RoundTrip(request).blueprintId, request.blueprintId);
	ExpectTruncatedFails(request);

	ClientPackets::UgcManifestResponse response;
	response.blueprintId = 0x0102030405060708;
	response.resourceType = eUgcResourceType::DDS;
	response.valid = true;
	response.fileSize = 65664;
	for (size_t i = 0; i < response.md5.size(); i++) response.md5[i] = static_cast<uint8_t>(0xa0 + i);
	RakNet::BitStream responseBytes;
	response.WritePacket(responseBytes);
	EXPECT_PACKET_EQ(FromHex("53 05 00 3c 00 00 00 00 08 07 06 05 04 03 02 01 03 01 80 00 01 00 "
		"a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af"), FromBitStream(responseBytes));
	EXPECT_EQ(responseBytes.GetNumberOfBytesUsed() - 1, 37);
	const auto read = RoundTrip(response);
	EXPECT_TRUE(read.valid);
	EXPECT_EQ(read.fileSize, 65664u);
	EXPECT_EQ(read.md5, response.md5);
	ExpectTruncatedFails(response);
}
