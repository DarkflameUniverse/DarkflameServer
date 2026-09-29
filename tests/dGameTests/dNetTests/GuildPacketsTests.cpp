// Guild packets (docs/Guilds.md): the bytes the 1.10.64 client reads (offsets from its packet handlers), the bytes it
// sends, and DLU's world <-> chat packets.
#include "ChatPackets.h"
#include "ClientPackets.h"
#include "WorldPackets.h"
#include "PlayerMessages.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include <gtest/gtest.h>

using namespace PacketTestUtils;
using GameMessageTestUtils::RoundTrip;
using GameMessageTestUtils::ExpectTruncatedFails;

namespace {
	// Little-endian bytes, the way the client's packed structs lie in memory
	struct Bytes {
		std::vector<uint8_t> data;
		Bytes& U8(uint8_t value) { data.push_back(value); return *this; }
		Bytes& U16(uint16_t value) { return U8(value & 0xFF).U8(value >> 8); }
		Bytes& U32(uint32_t value) { return U16(value & 0xFFFF).U16(value >> 16); }
		Bytes& U64(uint64_t value) { return U32(value & 0xFFFFFFFF).U32(value >> 32); }
		// A fixed buffer of `size` UTF-16 characters, zero filled
		Bytes& WStr(const std::u16string& text, size_t size) {
			for (size_t i = 0; i < size; i++) U16(i < text.size() ? text[i] : 0);
			return *this;
		}
		Bytes& Zone(uint16_t map, uint16_t instance, uint32_t clone) { return U16(map).U16(instance).U32(clone); }
		// 0x53, connection type, packet id, padding
		static Bytes Header(uint16_t connection, uint32_t id) { Bytes bytes; bytes.U8(0x53).U16(connection).U32(id).U8(0); return bytes; }
		PacketBytes Packet() const { return { data, static_cast<uint32_t>(data.size() * 8) }; }
	};

	PacketBytes StructPacket(const LUBitStream& msg) {
		RakNet::BitStream bitStream;
		msg.WritePacket(bitStream);
		return FromBitStream(bitStream);
	}

	constexpr uint16_t CLIENT = 5;
	constexpr uint16_t CHAT = 2;
	constexpr uint16_t WORLD = 4;
	constexpr LWOOBJID PLAYER = 0x1000000000000123LL;
	constexpr LWOOBJID OTHER = 0x1000000000000456LL;
}

class GuildPacketsTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

// ---- Server -> client. Offsets in the comments are the client's, from the byte after 0x53. ----

TEST_F(GuildPacketsTests, CreateResponse) {
	ClientPackets::GuildCreateResponse response;
	response.result = eGuildCreateResponse::CREATED;
	response.guildID = 7;
	response.guildName = "Brick Builders";
	// +7 result, +8 u64, +16 wchar[31]: 78 bytes after 0x53
	const auto expected = Bytes::Header(CLIENT, 37).U8(0).U64(7).WStr(u"Brick Builders", 31);
	EXPECT_EQ(expected.data.size(), 79u);
	EXPECT_PACKET_EQ(expected.Packet(), StructPacket(response));
	const auto copy = RoundTrip(response);
	EXPECT_EQ(copy.guildName, "Brick Builders");
	ExpectTruncatedFails(response);
}

TEST_F(GuildPacketsTests, NamesAlwaysKeepANul) {
	ClientPackets::GuildCreateResponse response;
	response.guildName = std::string(40, 'x');
	const auto packet = StructPacket(response);
	ASSERT_EQ(packet.bytes.size(), 79u);
	// the last character of the 31 is 0
	EXPECT_EQ(packet.bytes[77], 0);
	EXPECT_EQ(packet.bytes[78], 0);
	EXPECT_EQ(packet.bytes[75], 'x');
}

TEST_F(GuildPacketsTests, Invite) {
	ClientPackets::GuildInvite invite;
	invite.inviterName = "Alice";
	invite.guildName = "Brick Builders";
	// +7 wchar[33] inviter, +73 wchar[31] guild
	const auto expected = Bytes::Header(CLIENT, 39).WStr(u"Alice", 33).WStr(u"Brick Builders", 31);
	EXPECT_PACKET_EQ(expected.Packet(), StructPacket(invite));
	RoundTrip(invite);
}

TEST_F(GuildPacketsTests, InviteResponses) {
	ClientPackets::GuildInviteInitialResponse initial;
	initial.response = eGuildInviteResponse::INVITE_PENDING;
	initial.playerName = "Bob";
	EXPECT_PACKET_EQ(Bytes::Header(CLIENT, 40).U8(3).WStr(u"Bob", 33).Packet(), StructPacket(initial));
	RoundTrip(initial);

	ClientPackets::GuildInviteFinalResponse final;
	final.response = eGuildInviteFinalResponse::DECLINED;
	final.playerName = "Bob";
	EXPECT_PACKET_EQ(Bytes::Header(CLIENT, 41).U8(1).WStr(u"Bob", 33).Packet(), StructPacket(final));
	RoundTrip(final);

	ClientPackets::GuildInviteConfirm confirm;
	confirm.failed = true;
	confirm.guildName = "Brick Builders";
	EXPECT_PACKET_EQ(Bytes::Header(CLIENT, 42).U8(1).WStr(u"Brick Builders", 33).Packet(), StructPacket(confirm));
	EXPECT_TRUE(RoundTrip(confirm).failed);
}

TEST_F(GuildPacketsTests, AddPlayer) {
	ClientPackets::GuildAddPlayer add;
	add.playerName = "Bob";
	add.playerID = OTHER;
	add.rank = eGuildRank::RECRUIT;
	add.zoneID = LWOZONEID(1200, 3, 0);
	add.online = true;
	// +7 name, +73 id, +81 rank, +82 zone, +90 online: 91 bytes after 0x53
	const auto expected = Bytes::Header(CLIENT, 43).WStr(u"Bob", 33).U64(OTHER).U8(4).Zone(1200, 3, 0).U8(1);
	EXPECT_EQ(expected.data.size(), 92u);
	EXPECT_PACKET_EQ(expected.Packet(), StructPacket(add));
	const auto copy = RoundTrip(add);
	EXPECT_EQ(copy.zoneID, add.zoneID);
	ExpectTruncatedFails(add);
}

TEST_F(GuildPacketsTests, RemovePlayer) {
	ClientPackets::GuildRemovePlayer remove;
	remove.reason = eGuildLeaveReason::KICKED;
	remove.playerName = "Bob";
	remove.playerID = OTHER;
	remove.newLeaderID = PLAYER;
	// +7 reason, +8 name, +74 id, +82 new leader
	const auto expected = Bytes::Header(CLIENT, 44).U8(1).WStr(u"Bob", 33).U64(OTHER).U64(PLAYER);
	EXPECT_EQ(expected.data.size(), 91u);
	EXPECT_PACKET_EQ(expected.Packet(), StructPacket(remove));
	RoundTrip(remove);
}

TEST_F(GuildPacketsTests, LoginLogout) {
	ClientPackets::GuildLoginLogout login;
	login.playerName = "Bob";
	login.playerID = OTHER;
	login.online = true;
	login.zoneID = LWOZONEID(1100, 2, 0);
	login.worldUpdateOnly = true;
	// +7 name, +73 id, +81 online, +82 zone, +90 world update only
	const auto expected = Bytes::Header(CLIENT, 45).WStr(u"Bob", 33).U64(OTHER).U8(1).Zone(1100, 2, 0).U8(1);
	EXPECT_EQ(expected.data.size(), 92u);
	EXPECT_PACKET_EQ(expected.Packet(), StructPacket(login));
	RoundTrip(login);
}

TEST_F(GuildPacketsTests, Data) {
	ClientPackets::GuildData data;
	data.guildName = "Brick Builders";
	data.joinDate = "01/02/2026";
	data.foundDate = "12/31/2025";
	data.members.push_back({ eGuildRank::LEADER, true, LWOZONEID(1200, 1, 0), PLAYER, "Alice" });
	data.members.push_back({ eGuildRank::RECRUIT, false, LWOZONEID(0, 0, 0), OTHER, "Bob" });
	// +7 status, +8 name, +70 date, +92 date, +114 reputation, +118..127, +128 count, +130 members of 84 bytes
	auto expected = Bytes::Header(CLIENT, 47).U8(0).WStr(u"Brick Builders", 31).WStr(u"01/02/2026", 11).WStr(u"12/31/2025", 11)
		.U32(0).U32(0).U32(0).U16(0).U16(2);
	EXPECT_EQ(expected.data.size(), 131u);
	expected.U8(1).U8(1).Zone(1200, 1, 0).U64(PLAYER).WStr(u"Alice", 33);
	expected.U8(4).U8(0).Zone(0, 0, 0).U64(OTHER).WStr(u"Bob", 33);
	EXPECT_EQ(expected.data.size(), 131u + 2 * 84u);
	EXPECT_PACKET_EQ(expected.Packet(), StructPacket(data));
	const auto copy = RoundTrip(data);
	ASSERT_EQ(copy.members.size(), 2u);
	EXPECT_EQ(copy.members[0].name, "Alice");
	EXPECT_EQ(copy.members[1].playerID, OTHER);
	ExpectTruncatedFails(data);
}

// ---- Client -> server ----

TEST_F(GuildPacketsTests, TmpGuildCreateFromClient) {
	// What SendTMPGuildCreate sends (69 bytes after 0x53): header, wchar[31]
	const auto bytes = Bytes::Header(WORLD, 20).WStr(u"Brick Builders", 31);
	RakNet::BitStream stream(const_cast<unsigned char*>(bytes.data.data()), bytes.data.size(), false);
	LUBitStream header;
	ASSERT_TRUE(header.ReadHeader(stream));
	EXPECT_EQ(header.connectionType, ServiceType::WORLD);
	EXPECT_EQ(header.internalPacketID, static_cast<uint32_t>(MessageType::World::TMP_GUILD_CREATE));
	WorldPackets::TmpGuildCreate create;
	ASSERT_TRUE(create.Deserialize(stream));
	EXPECT_EQ(create.guildName, u"Brick Builders");
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
	EXPECT_PACKET_EQ(bytes.Packet(), StructPacket(create));
}

TEST_F(GuildPacketsTests, RoutedFromClientToChat) {
	// The client's GUILD_INVITE (81 bytes after 0x53): header, u64 0, wchar[33]; the world routes it to chat with the
	// sender in front and the first 4 bytes of the data dropped
	WorldPackets::RoutePacket route;
	const auto routed = Bytes().U64(0).WStr(u"Bob", 33);
	route.size = static_cast<uint32_t>(routed.data.size());
	route.routedService = ServiceType::CHAT;
	route.routedMessageID = static_cast<uint32_t>(MessageType::Chat::GUILD_INVITE);
	route.routedData = routed.data;
	RakNet::BitStream bitStream;
	route.ToChat(PLAYER).WritePacket(bitStream);
	bitStream.ResetReadPointer();
	LUBitStream header;
	ASSERT_TRUE(header.ReadHeader(bitStream));
	EXPECT_EQ(header.internalPacketID, static_cast<uint32_t>(MessageType::Chat::GUILD_INVITE));
	ChatPackets::GuildInvite invite;
	ASSERT_TRUE(invite.Deserialize(bitStream));
	EXPECT_EQ(invite.playerID, PLAYER);
	EXPECT_EQ(invite.invitedPlayer.GetAsString(), "Bob");

	// GUILD_INVITE_RESPONSE (16 bytes): u64 0, u8 declined
	route.routedMessageID = static_cast<uint32_t>(MessageType::Chat::GUILD_INVITE_RESPONSE);
	route.routedData = Bytes().U64(0).U8(1).data;
	route.size = 9;
	RakNet::BitStream responseStream;
	route.ToChat(PLAYER).WritePacket(responseStream);
	ASSERT_TRUE(header.ReadHeader(responseStream));
	ChatPackets::GuildInviteResponse response;
	ASSERT_TRUE(response.Deserialize(responseStream));
	EXPECT_EQ(response.playerID, PLAYER);
	EXPECT_EQ(response.declined, 1);

	// GUILD_LEAVE: the client says it is 81 bytes long; everything after the u64 is ignored
	route.routedMessageID = static_cast<uint32_t>(MessageType::Chat::GUILD_LEAVE);
	route.routedData = Bytes().U64(0).WStr(u"garbage", 33).data;
	route.size = 74;
	RakNet::BitStream leaveStream;
	route.ToChat(PLAYER).WritePacket(leaveStream);
	ASSERT_TRUE(header.ReadHeader(leaveStream));
	ChatPackets::GuildLeave leave;
	ASSERT_TRUE(leave.Deserialize(leaveStream));
	EXPECT_EQ(leave.playerID, PLAYER);

	// GUILD_GET_ALL (15 bytes): u64 0
	route.routedMessageID = static_cast<uint32_t>(MessageType::Chat::GUILD_GET_ALL);
	route.routedData = Bytes().U64(0).data;
	route.size = 8;
	RakNet::BitStream getAllStream;
	route.ToChat(PLAYER).WritePacket(getAllStream);
	ASSERT_TRUE(header.ReadHeader(getAllStream));
	ChatPackets::GuildGetAll getAll;
	ASSERT_TRUE(getAll.Deserialize(getAllStream));
	EXPECT_EQ(getAll.playerID, PLAYER);
	EXPECT_EQ(getAllStream.GetNumberOfUnreadBits(), 0u);
}

// ---- DLU's world <-> chat packets ----

TEST_F(GuildPacketsTests, WorldChatPackets) {
	ChatPackets::GuildCreate create;
	create.playerID = PLAYER;
	create.guildName = LUWString(u"Brick Builders", 31);
	EXPECT_PACKET_EQ(Bytes::Header(CHAT, 22).U64(PLAYER).WStr(u"Brick Builders", 31).Packet(), StructPacket(create));
	RoundTrip(create);

	ChatPackets::GuildKick kick;
	kick.playerID = PLAYER;
	kick.kickedPlayer = LUWString(u"Bob", 33);
	EXPECT_PACKET_EQ(Bytes::Header(CHAT, 26).U64(PLAYER).U32(0).WStr(u"Bob", 33).Packet(), StructPacket(kick));
	RoundTrip(kick);

	ChatPackets::GuildSetRank rank;
	rank.playerID = PLAYER;
	rank.targetPlayer = LUWString(u"Bob", 33);
	rank.rank = 2;
	EXPECT_EQ(static_cast<uint32_t>(MessageType::Chat::GUILD_SET_RANK), static_cast<uint32_t>(MessageType::Chat::CREATE_TEAM) + 1);
	RoundTrip(rank);

	ChatPackets::GuildDisband disband;
	disband.playerID = PLAYER;
	EXPECT_EQ(static_cast<uint32_t>(MessageType::Chat::GUILD_DISBAND), static_cast<uint32_t>(MessageType::Chat::CREATE_TEAM) + 2);
	RoundTrip(disband);

	ChatPackets::GuildStatus status;
	status.characterID = PLAYER;
	status.guildID = 7;
	status.guildName = LUWString(u"Brick Builders", 31);
	EXPECT_PACKET_EQ(Bytes::Header(CHAT, 27).U64(PLAYER).U64(7).WStr(u"Brick Builders", 31).Packet(), StructPacket(status));
	RoundTrip(status);
}

// ---- Game message ----

TEST_F(GuildPacketsTests, DisplayGuildCreateBox) {
	GameMessages::DisplayGuildCreateBox box;
	box.target = PLAYER;
	// header 53 05 00 0c 00 00 00 00, target, msgId 626 (u16), bShow bit
	EXPECT_PACKET_EQ(FromHex("5305000c00000000" "2301000000000010" "7202" "80", 145), GameMessageTestUtils::StructPacket(box));
	RoundTrip(box);
}
