#include "ChatPackets.h"
#include "ClientPackets.h"
#include "ActivityMessages.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include <gtest/gtest.h>

using namespace PacketTestUtils;
using GameMessageTestUtils::RoundTrip;
using GameMessageTestUtils::ExpectTruncatedFails;

// Packets of the chat server's activity matchmaking (docs/Matchmaking.md)

TEST(MatchmakingPacketsTests, MessageIdsAreAppended) {
	// MATCH_REQUEST is the chat service's own id; MATCH_TRANSFER is DLU's, after GUILD_DISBAND
	EXPECT_EQ(static_cast<uint32_t>(MessageType::Chat::MATCH_REQUEST), 52u);
	EXPECT_EQ(static_cast<uint32_t>(MessageType::Chat::MATCH_TRANSFER), static_cast<uint32_t>(MessageType::Chat::GUILD_DISBAND) + 1);
	EXPECT_EQ(ChatPackets::MatchRequest().internalPacketID, static_cast<uint32_t>(MessageType::Chat::MATCH_REQUEST));
	EXPECT_EQ(ChatPackets::MatchTransfer().internalPacketID, static_cast<uint32_t>(MessageType::Chat::MATCH_TRANSFER));
}

TEST(MatchmakingPacketsTests, MatchRequestRoundTrip) {
	for (const auto type : { ChatPackets::eMatchRequestType::JOIN, ChatPackets::eMatchRequestType::READY, ChatPackets::eMatchRequestType::LEAVE }) {
		ChatPackets::MatchRequest request;
		request.playerID = 0x1000000012345678LL;
		request.type = type;
		request.value = 1;
		request.activityID = 42;
		request.playerName = "GruntMonkey";
		request.playerChoices = "droppedItem=13:1152921510659010409";
		request.instanceMapID = 1203;
		request.minTeams = 2;
		request.maxTeams = 6;
		request.minTeamSize = 1;
		request.maxTeamSize = 1;
		request.waitTime = 60000;
		request.startDelay = 3000;

		const auto copy = RoundTrip(request);
		EXPECT_EQ(copy.playerID, request.playerID);
		EXPECT_EQ(copy.type, type);
		EXPECT_EQ(copy.value, 1);
		EXPECT_EQ(copy.activityID, 42);
		EXPECT_EQ(copy.playerName, "GruntMonkey");
		EXPECT_EQ(copy.playerChoices, request.playerChoices);
		EXPECT_EQ(copy.instanceMapID, 1203u);
		EXPECT_EQ(copy.minTeams, 2);
		EXPECT_EQ(copy.maxTeams, 6);
		EXPECT_EQ(copy.minTeamSize, 1);
		EXPECT_EQ(copy.maxTeamSize, 1);
		EXPECT_EQ(copy.waitTime, 60000);
		EXPECT_EQ(copy.startDelay, 3000);
		ExpectTruncatedFails(request);
	}
}

TEST(MatchmakingPacketsTests, MatchRequestRejectsUnknownTypeAndLongName) {
	ChatPackets::MatchRequest request;
	request.type = static_cast<ChatPackets::eMatchRequestType>(7);
	RakNet::BitStream bitStream;
	request.Serialize(bitStream);
	ChatPackets::MatchRequest copy;
	EXPECT_FALSE(copy.Deserialize(bitStream));

	request.type = ChatPackets::eMatchRequestType::JOIN;
	request.playerName = std::string(ChatPackets::LoginSessionNotify::MAX_NAME_LENGTH + 1, 'x');
	RakNet::BitStream longName;
	request.Serialize(longName);
	EXPECT_FALSE(copy.Deserialize(longName));
}

TEST(MatchmakingPacketsTests, MatchTransferRoundTrip) {
	ChatPackets::MatchTransfer transfer;
	transfer.activityID = 5;
	transfer.zoneID = LWOZONEID(1101, 42, 0xDEADBEEF);
	transfer.serverIP = "171.20.35.21";
	transfer.serverPort = 2007;
	transfer.mythranShift = false;
	transfer.players = { 0x1000000000000001LL, 0x1000000000000002LL };

	const auto copy = RoundTrip(transfer);
	EXPECT_EQ(copy.activityID, 5);
	EXPECT_EQ(copy.zoneID.GetMapID(), 1101);
	EXPECT_EQ(copy.zoneID.GetInstanceID(), 42);
	EXPECT_EQ(copy.zoneID.GetCloneID(), 0xDEADBEEFu);
	EXPECT_EQ(copy.serverIP, "171.20.35.21");
	EXPECT_EQ(copy.serverPort, 2007);
	EXPECT_FALSE(copy.mythranShift);
	EXPECT_EQ(copy.players, transfer.players);
	ExpectTruncatedFails(transfer);
}

TEST(MatchmakingPacketsTests, MatchTransferRejectsTooManyPlayers) {
	ChatPackets::MatchTransfer transfer;
	transfer.players.assign(ChatPackets::MatchTransfer::MAX_PLAYERS + 1, 1);
	RakNet::BitStream bitStream;
	transfer.Serialize(bitStream);
	ChatPackets::MatchTransfer copy;
	EXPECT_FALSE(copy.Deserialize(bitStream));
}

// The chat server's MatchUpdate is byte for byte the world's GameMessages::MatchUpdate
TEST(MatchmakingPacketsTests, ClientMatchUpdateMatchesGameMessage) {
	const std::pair<std::string, eMatchUpdate> cases[] = {
		{ "player=9:1152921510436607007\nplayerName=0:GruntMonkey", eMatchUpdate::PLAYER_ADDED },
		{ "droppedItem=9:1152921510659010409\nplayer=9:1152921510436607007\nplayerName=0:GruntMonkey", eMatchUpdate::PLAYER_ADDED },
		{ "player=9:1152921510808957955", eMatchUpdate::PLAYER_READY },
		{ "time=3:49.704", eMatchUpdate::PHASE_WAIT_READY },
		{ "", eMatchUpdate::PLAYER_REMOVED },
	};
	for (const auto& [data, type] : cases) {
		GameMessages::MatchUpdate world;
		world.target = 0x1000000012345678LL;
		world.data = data;
		world.type = type;
		RakNet::BitStream worldStream;
		world.WritePacket(worldStream);

		ClientPackets::MatchUpdate chat;
		chat.target = world.target;
		chat.data = data;
		chat.type = type;
		RakNet::BitStream chatStream;
		chat.WritePacket(chatStream);

		EXPECT_PACKET_EQ(FromBitStream(worldStream), FromBitStream(chatStream));

		const auto copy = RoundTrip(chat);
		EXPECT_EQ(copy.target, chat.target);
		EXPECT_EQ(copy.data, data);
		EXPECT_EQ(copy.type, type);
	}
}
