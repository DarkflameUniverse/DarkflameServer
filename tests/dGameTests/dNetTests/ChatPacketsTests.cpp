#include "ChatPackets.h"
#include "ClientPackets.h"
#include "WorldRoutePacket.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"
#include "Legacy/ChatPacketsLegacy.h"

#include <array>
#include <functional>
#include <limits>

#include <gtest/gtest.h>

using namespace PacketTestUtils;
using GameMessageTestUtils::RoundTrip;
using GameMessageTestUtils::ExpectTruncatedFails;

namespace {
	SystemAddress WorldAddress() {
		SystemAddress address;
		address.binaryAddress = 0x0100007f;
		address.port = 3000;
		return address;
	}

	const std::array<LWOOBJID, 3> g_Ids = { LWOOBJID_EMPTY, 0x1000000000000001LL, 0x0102030405060708LL };
	const std::array<std::string, 4> g_Names = { "", "Bob", "A name with 33 characters in it!!", "Th\xc3\xa9o" };
	const std::array<std::u16string, 4> g_Messages = { u"", u"hello", u"café ☃", std::u16string(300, u'x') };
	const std::array<LWOZONEID, 3> g_Zones = { LWOZONEID(), LWOZONEID(1100, 3, 0), LWOZONEID(1150, 65535, 0xDEADBEEF) };

	PacketBytes StructPacket(const LUBitStream& msg) {
		RakNet::BitStream bitStream;
		msg.WritePacket(bitStream);
		return FromBitStream(bitStream);
	}

	// What ChatPacketHandler::SendRouted puts on the wire
	PacketBytes Routed(LWOOBJID target, const LUBitStream& msg) {
		ChatPackets::WorldRoutePacket route;
		route.targetID = target;
		route.routed = &msg;
		return StructPacket(route);
	}

	// The one packet the legacy function sent
	CapturedPacket LegacyPacket(const std::function<void()>& legacySend) {
		const auto packets = Capture(legacySend);
		EXPECT_EQ(packets.size(), 1);
		return packets.empty() ? CapturedPacket{} : packets[0];
	}

	PacketBytes Written(const std::function<void(RakNet::BitStream&)>& write) {
		RakNet::BitStream bitStream;
		write(bitStream);
		return FromBitStream(bitStream);
	}

	// A stream positioned after the header of msg's packet
	void LoadPayload(RakNet::BitStream& bitStream, const LUBitStream& msg) {
		msg.WritePacket(bitStream);
		bitStream.IgnoreBytes(8);
	}

	PlayerData MakePlayer(LWOOBJID id, const std::string& name, LWOZONEID zone, eGameMasterLevel gmLevel = eGameMasterLevel::CIVILIAN) {
		PlayerData player;
		player.playerID = id;
		player.playerName = name;
		player.zoneID = zone;
		player.gmLevel = gmLevel;
		player.worldServerSysAddr = WorldAddress();
		return player;
	}
}

class ChatPacketsTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

// Client-facing: general chat and system messages

TEST_F(ChatPacketsTests, GeneralChatMessageMatchesLegacySendChatMessage) {
	for (const uint8_t channel : { 0, 4, 12, 255 }) {
		for (const auto& name : g_Names) {
			for (const auto id : g_Ids) {
				for (const auto& message : g_Messages) {
					for (const bool mythran : { false, true }) {
						const auto legacy = LegacyPacket([&] { LegacyChat::SendChatMessage(WorldAddress(), static_cast<char>(channel), name, id, mythran, message); });

						ChatPackets::Client::GeneralChatMessage chatMessage;
						chatMessage.chatChannel = channel;
						chatMessage.senderName = LUWString(name);
						chatMessage.senderID = id;
						chatMessage.message = message;
						const auto sent = LegacyPacket([&] { chatMessage.Broadcast(); });

						EXPECT_PACKET_EQ(FromCapture(legacy), FromCapture(sent));
						EXPECT_EQ(legacy.broadcast, sent.broadcast);
						EXPECT_EQ(legacy.sysAddr, sent.sysAddr);

						const auto copy = RoundTrip(chatMessage);
						EXPECT_EQ(copy.message, message);
						EXPECT_EQ(copy.senderID, id);
						EXPECT_EQ(copy.chatChannel, channel);
					}
				}
			}
		}
	}
}

TEST_F(ChatPacketsTests, SendSystemMessageMatchesLegacy) {
	for (const auto& address : { WorldAddress(), UNASSIGNED_SYSTEM_ADDRESS }) {
		for (const bool broadcast : { false, true }) {
			for (const auto& message : g_Messages) {
				const auto legacy = LegacyPacket([&] { LegacyChat::SendSystemMessage(address, message, broadcast); });
				const auto sent = LegacyPacket([&] { ChatPackets::SendSystemMessage(address, message, broadcast); });
				EXPECT_PACKET_EQ(FromCapture(legacy), FromCapture(sent));
				EXPECT_EQ(legacy.broadcast, sent.broadcast);
				EXPECT_EQ(legacy.sysAddr, sent.sysAddr);
			}
			const auto legacy = LegacyPacket([&] { LegacyChat::SendSystemMessage(address, std::string("ascii text"), broadcast); });
			const auto sent = LegacyPacket([&] { ChatPackets::SendSystemMessage(address, std::string("ascii text"), broadcast); });
			EXPECT_PACKET_EQ(FromCapture(legacy), FromCapture(sent));
		}
	}
}

TEST_F(ChatPacketsTests, GeneralChatMessageGolden) {
	ChatPackets::Client::GeneralChatMessage chatMessage;
	chatMessage.chatChannel = 4;
	chatMessage.senderName = LUWString("", 33);
	chatMessage.message = u"hi";
	const auto packet = StructPacket(chatMessage);
	// 53 | CHAT (0x0002) | GENERAL_CHAT_MESSAGE (1) | pad | u64 0 | channel 4 | u32 length 2 | 66 zero bytes of name |
	// u64 sender 0 | u16 source 0 | u8 gm level 0 | 'h' 'i' | u16 0
	std::string hex = "53 02 00 01 00 00 00 00 00 00 00 00 00 00 00 00 04 02 00 00 00";
	for (int i = 0; i < 66; i++) hex += " 00";
	hex += " 00 00 00 00 00 00 00 00 00 00 00 68 00 69 00 00 00";
	EXPECT_PACKET_EQ(FromHex(hex), packet);
}

TEST_F(ChatPacketsTests, SendCannedTextMatchesLegacySendMessageFail) {
	const auto legacy = LegacyPacket([&] { LegacyChat::SendMessageFail(WorldAddress()); });
	ClientPackets::SendCannedText cannedText;
	const auto sent = LegacyPacket([&] { cannedText.Send(WorldAddress()); });
	EXPECT_PACKET_EQ(FromCapture(legacy), FromCapture(sent));
	EXPECT_EQ(legacy.broadcast, sent.broadcast);
	EXPECT_EQ(legacy.sysAddr, sent.sysAddr);
	EXPECT_PACKET_EQ(FromHex("53 05 00 35 00 00 00 00 00"), StructPacket(cannedText));
	RoundTrip(cannedText);
}

// Routed packets from the chat server

TEST_F(ChatPacketsTests, WorldRoutePacketMatchesLegacySendRoutedMsg) {
	for (const auto target : g_Ids) {
		for (const bool failed : { false, true }) {
			ClientPackets::TeamInviteInitialResponse response;
			response.inviteFailedToSend = failed;
			response.playerName = LUWString("Bob");

			RakNet::BitStream inner;
			LegacyChat::WriteTeamInviteInitialResponse(inner, failed, LUWString("Bob"));
			EXPECT_PACKET_EQ(FromBitStream(inner), StructPacket(response));

			const auto legacy = LegacyPacket([&] { LegacyChat::SendRoutedMsg(inner, target, WorldAddress()); });
			EXPECT_PACKET_EQ(FromCapture(legacy), Routed(target, response));
			EXPECT_FALSE(legacy.broadcast);

			const auto copy = RoundTrip(response);
			EXPECT_EQ(copy.inviteFailedToSend, failed);
		}
	}
}

TEST_F(ChatPacketsTests, WorldRoutePacketGoldenAndWorldForwarding) {
	ClientPackets::TeamSetLeader msg;
	msg.target = 0x0102030405060708LL;
	msg.i64PlayerID = 0x1112131415161718LL;
	const auto packet = Routed(0x0102030405060708LL, msg);
	// WORLD_ROUTE_PACKET (0x30) | target | CLIENT GAME_MSG (0x0c) | target | TEAM_SET_LEADER (1557 = 0x0615) | player
	EXPECT_PACKET_EQ(FromHex(
		"53 02 00 30 00 00 00 00 08 07 06 05 04 03 02 01 "
		"53 05 00 0c 00 00 00 00 08 07 06 05 04 03 02 01 15 06 18 17 16 15 14 13 12 11"), packet);

	// The world reads the target and passes everything after it on unchanged
	RakNet::BitStream bitStream;
	bitStream.WriteBits(packet.bytes.data(), packet.bits);
	RakNet::BitStream legacyStream;
	legacyStream.WriteBits(packet.bytes.data(), packet.bits);
	bitStream.IgnoreBytes(8);
	legacyStream.IgnoreBytes(8);

	ChatPackets::WorldRoutePacket route;
	ASSERT_TRUE(route.Deserialize(bitStream));
	const auto [legacyTarget, legacyBytes] = LegacyChat::ReadWorldRoute(legacyStream);
	EXPECT_EQ(route.targetID, legacyTarget);
	EXPECT_EQ(route.routedData, legacyBytes);
	EXPECT_EQ(route.routedData, StructPacket(msg).bytes);

	// And serializing what was read gives the same packet
	EXPECT_PACKET_EQ(packet, StructPacket(route));
}

TEST_F(ChatPacketsTests, GetFriendsListResponseMatchesLegacy) {
	for (const auto target : g_Ids) {
		for (size_t count = 0; count <= 3; count++) {
			PlayerData player = MakePlayer(target, "Me", g_Zones[1]);
			for (size_t i = 0; i < count; i++) {
				FriendData data;
				data.isOnline = i % 2 == 0;
				data.isBestFriend = i == 1;
				data.isFTP = i == 2;
				data.zoneID = g_Zones[i % g_Zones.size()];
				data.friendID = g_Ids[i % g_Ids.size()];
				data.friendName = g_Names[i % 3]; // names of up to 33 characters
				player.friends.push_back(data);
			}

			const auto legacy = LegacyPacket([&] { LegacyChat::SendFriendsList(target, player); });
			ClientPackets::GetFriendsListResponse response;
			response.friends = player.friends;
			EXPECT_PACKET_EQ(FromCapture(legacy), Routed(target, response));
			EXPECT_EQ(legacy.sysAddr, WorldAddress());
			EXPECT_FALSE(legacy.broadcast);

			const auto copy = RoundTrip(response);
			ASSERT_EQ(copy.friends.size(), count);
			for (size_t i = 0; i < count; i++) {
				EXPECT_EQ(copy.friends[i].friendName, player.friends[i].friendName);
				EXPECT_EQ(copy.friends[i].zoneID, player.friends[i].zoneID);
			}
		}
	}
}

TEST_F(ChatPacketsTests, WhoResponseMatchesLegacy) {
	for (const auto requestor : g_Ids) {
		for (const auto& name : g_Names) {
			for (const bool online : { false, true }) {
				LegacyChat::FindPlayerRequest legacyRequest{ requestor, LUWString(name) };
				const PlayerData sender = MakePlayer(requestor, "Asker", g_Zones[1]);
				const PlayerData player = online ? MakePlayer(0x55, name, g_Zones[2]) : PlayerData();

				const auto legacy = LegacyPacket([&] { LegacyChat::SendWhoResponse(legacyRequest, sender, player); });
				ClientPackets::WhoResponse response;
				response.isOnline = static_cast<bool>(player);
				response.zoneID = player.zoneID;
				response.playerName = LUWString(name);
				EXPECT_PACKET_EQ(FromCapture(legacy), Routed(requestor, response));
				RoundTrip(response);
				ExpectTruncatedFails(response);
			}
		}
	}
}

TEST_F(ChatPacketsTests, ShowAllResponseMatchesLegacy) {
	std::map<LWOOBJID, PlayerData> players;
	players[LWOOBJID_EMPTY] = PlayerData(); // the container's empty entry is skipped
	players[1] = MakePlayer(1, "One", g_Zones[1]);
	players[2] = MakePlayer(2, "Two", g_Zones[2]);
	players[3] = MakePlayer(3, "", g_Zones[0]);

	for (const bool zoneData : { false, true }) {
		for (const bool individualPlayers : { false, true }) {
			for (const uint32_t playerCount : { 0u, 3u, 0xFFFFFFFFu }) {
				LegacyChat::ShowAllRequest legacyRequest{ 0x42, zoneData, individualPlayers };
				const PlayerData sender = MakePlayer(0x42, "Asker", g_Zones[1]);

				const auto legacy = LegacyPacket([&] { LegacyChat::SendShowAllResponse(legacyRequest, sender, playerCount, 7, players); });

				ClientPackets::ShowAllResponse response;
				response.playerCount = playerCount;
				response.simCount = 7;
				response.displayIndividualPlayers = individualPlayers;
				response.displayZoneData = zoneData;
				if (zoneData || individualPlayers) {
					for (auto& [playerID, playerData] : players) {
						if (!playerData) continue;
						response.players.push_back({ playerData.playerName, playerData.zoneID });
					}
				}
				EXPECT_PACKET_EQ(FromCapture(legacy), Routed(0x42, response));

				const auto copy = RoundTrip(response);
				EXPECT_EQ(copy.players.size(), response.players.size());
			}
		}
	}
}

TEST_F(ChatPacketsTests, PrivateChatMessageMatchesLegacy) {
	for (const auto& senderName : g_Names) {
		for (const auto gmLevel : { eGameMasterLevel::CIVILIAN, eGameMasterLevel::OPERATOR }) {
			for (const auto responseCode : { eChatMessageResponseCode::SENT, eChatMessageResponseCode::NOTFRIENDS, eChatMessageResponseCode::RECEIVERFREETRIAL }) {
				for (const auto channel : { eChatChannel::PRIVATE_CHAT, eChatChannel::TEAM, eChatChannel::GENERAL }) {
					for (const uint32_t size : { 0u, 5u, 40u }) {
						const PlayerData sender = MakePlayer(0x0102030405060708LL, senderName, g_Zones[1], gmLevel);
						const PlayerData receiver = MakePlayer(0x77, "Receiver", g_Zones[2], eGameMasterLevel::DEVELOPER);
						LUWString message(u"hello", size);

						const auto legacy = LegacyPacket([&] { LegacyChat::SendPrivateChatMessage(sender, receiver, receiver, message, channel, responseCode); });

						ChatPackets::Client::PrivateChatMessage chatMessage;
						chatMessage.playerID = sender.playerID;
						chatMessage.chatChannel = channel;
						chatMessage.senderName = LUWString(sender.playerName);
						chatMessage.senderID = sender.playerID;
						chatMessage.senderGMLevel = sender.gmLevel;
						chatMessage.receiverName = LUWString(receiver.playerName);
						chatMessage.receiverGMLevel = receiver.gmLevel;
						chatMessage.responseCode = responseCode;
						chatMessage.message = message;
						EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, chatMessage));

						const auto copy = RoundTrip(chatMessage);
						EXPECT_EQ(copy.message.size, size);
						EXPECT_EQ(copy.responseCode, responseCode);
					}
				}
			}
		}
	}
}

TEST_F(ChatPacketsTests, UpdateFriendNotifyMatchesLegacy) {
	for (const auto& zone : g_Zones) {
		for (const auto& friendZone : g_Zones) {
			for (const uint8_t notifyType : { 0, 1, 2 }) {
				for (const uint8_t isBestFriend : { 0, 1 }) {
					const PlayerData friendData = MakePlayer(0x10, "Friend", friendZone);
					const PlayerData playerData = MakePlayer(0x20, "Player", zone);

					const auto legacy = LegacyPacket([&] { LegacyChat::SendFriendUpdate(friendData, playerData, notifyType, isBestFriend); });

					// As ChatPacketHandler::SendFriendUpdate fills it in
					ClientPackets::UpdateFriendNotify notify;
					notify.notifyType = notifyType;
					notify.friendName = LUWString(playerData.playerName);
					const LWOCLONEID cloneID = zone.GetCloneID() == friendData.zoneID.GetCloneID() ? 0 : zone.GetCloneID();
					notify.zoneID = LWOZONEID(zone.GetMapID(), zone.GetInstanceID(), cloneID);
					notify.isBestFriend = isBestFriend;
					EXPECT_PACKET_EQ(FromCapture(legacy), Routed(friendData.playerID, notify));
					RoundTrip(notify);
					ExpectTruncatedFails(notify);
				}
			}
		}
	}
}

TEST_F(ChatPacketsTests, AddFriendRequestAndResponseMatchLegacy) {
	for (const auto& name : g_Names) {
		const PlayerData receiver = MakePlayer(0x10, "Receiver", g_Zones[1]);
		PlayerData sender = MakePlayer(0x20, name, g_Zones[2]);

		const auto legacy = LegacyPacket([&] { LegacyChat::SendFriendRequest(receiver, sender); });
		ClientPackets::AddFriendRequest request;
		request.requestorName = LUWString(sender.playerName);
		EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, request));
		RoundTrip(request);

		for (int code = 0; code <= static_cast<int>(eAddFriendResponseType::FRIENDISFREETRIAL); code++) {
			const auto responseCode = static_cast<eAddFriendResponseType>(code);
			for (const bool senderOnline : { false, true }) {
				for (const uint8_t alreadyBest : { 0, 1 }) {
					for (const uint8_t bestRequest : { 0, 1 }) {
						sender.worldServerSysAddr = senderOnline ? WorldAddress() : UNASSIGNED_SYSTEM_ADDRESS;
						const auto legacyResponse = LegacyPacket([&] { LegacyChat::SendFriendResponse(receiver, sender, responseCode, alreadyBest, bestRequest); });

						// As ChatPacketHandler::SendFriendResponse fills it in
						ClientPackets::AddFriendResponse response;
						response.responseCode = responseCode;
						response.isOnlineOrBestFriend = responseCode != eAddFriendResponseType::ACCEPTED ? alreadyBest : sender.worldServerSysAddr != UNASSIGNED_SYSTEM_ADDRESS;
						response.friendName = LUWString(sender.playerName);
						response.friendID = sender.playerID;
						response.zoneID = sender.zoneID;
						response.isBestFriend = bestRequest;
						EXPECT_PACKET_EQ(FromCapture(legacyResponse), Routed(receiver.playerID, response));
						RoundTrip(response);
						ExpectTruncatedFails(response);
					}
				}
			}
		}
	}
}

TEST_F(ChatPacketsTests, RemoveFriendResponseMatchesLegacy) {
	for (auto name : g_Names) {
		for (const bool success : { false, true }) {
			const PlayerData receiver = MakePlayer(0x10, "Receiver", g_Zones[1]);
			const auto legacy = LegacyPacket([&] { LegacyChat::SendRemoveFriend(receiver, name, success); });
			ClientPackets::RemoveFriendResponse response;
			response.isSuccessful = success;
			response.friendName = LUWString(name);
			EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, response));
			RoundTrip(response);
		}
	}
}

TEST_F(ChatPacketsTests, IgnoreListResponsesMatchLegacy) {
	for (size_t count = 0; count <= 3; count++) {
		PlayerData receiver = MakePlayer(0x10, "Receiver", g_Zones[1]);
		for (size_t i = 0; i < count; i++) receiver.ignoredPlayers.emplace_back(g_Names[i], g_Ids[i]);

		const auto legacy = LegacyPacket([&] { LegacyChat::SendIgnoreList(receiver, WorldAddress()); });
		ClientPackets::GetIgnoreListResponse response;
		for (const auto& ignoredPlayer : receiver.ignoredPlayers) {
			response.ignored.push_back({ ignoredPlayer.playerId, LUWString(ignoredPlayer.playerName, 36) });
		}
		EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, response));
		EXPECT_EQ(legacy.sysAddr, WorldAddress());
		EXPECT_FALSE(legacy.broadcast);
		RoundTrip(response);
	}

	const PlayerData receiver = MakePlayer(0x10, "Receiver", g_Zones[1]);
	for (int code = 0; code <= 3; code++) {
		for (const auto& name : g_Names) {
			for (const auto id : g_Ids) {
				const auto legacy = LegacyPacket([&] { LegacyChat::SendAddIgnoreResponse(receiver, static_cast<LegacyChat::AddResponse>(code), name, id, WorldAddress()); });
				ClientPackets::AddIgnoreResponse response;
				response.responseCode = static_cast<eAddIgnoreResponse>(code);
				response.playerName = LUWString(name, 33);
				response.playerID = id;
				EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, response));
				RoundTrip(response);
				ExpectTruncatedFails(response);
			}
		}
	}

	for (const auto& name : g_Names) {
		const auto legacy = LegacyPacket([&] { LegacyChat::SendRemoveIgnoreResponse(receiver, name, WorldAddress()); });
		ClientPackets::RemoveIgnoreResponse response;
		response.playerName = LUWString(name, 33);
		EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, response));
		RoundTrip(response);
	}
}

// Team packets and team game messages

TEST_F(ChatPacketsTests, TeamInviteMatchesLegacy) {
	for (const auto& name : g_Names) {
		for (const auto id : g_Ids) {
			const PlayerData receiver = MakePlayer(0x10, "Receiver", g_Zones[1]);
			const PlayerData sender = MakePlayer(id, name, g_Zones[2]);
			const auto legacy = LegacyPacket([&] { LegacyChat::SendTeamInvite(receiver, sender); });
			ClientPackets::TeamInvite invite;
			invite.senderName = LUWString(sender.playerName.c_str());
			invite.senderID = sender.playerID;
			EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, invite));
			RoundTrip(invite);
			ExpectTruncatedFails(invite);
		}
	}
}

TEST_F(ChatPacketsTests, TeamGameMessagesMatchLegacy) {
	const std::array<std::u16string, 3> names = { u"", u"Leader", u"Léader with a much longer name than usual" };
	for (const auto receiverId : g_Ids) {
		for (const auto& zone : g_Zones) {
			const PlayerData receiver = MakePlayer(receiverId, "Receiver", g_Zones[1]);
			for (const auto& name : names) {
				for (const bool flag : { false, true }) {
					for (const uint8_t byte : { 0, 1, 255 }) {
						{
							const auto legacy = LegacyPacket([&] { LegacyChat::SendTeamInviteConfirm(receiver, flag, 0x42, zone, byte, byte, byte, name); });
							ClientPackets::TeamInviteConfirm msg;
							msg.target = receiver.playerID;
							msg.bLeaderIsFreeTrial = flag;
							msg.i64LeaderID = 0x42;
							msg.i64LeaderZoneID = zone;
							msg.ucLootFlag = byte;
							msg.ucNumOfOtherPlayers = byte;
							msg.ucResponseCode = byte;
							msg.wsLeaderName = name;
							EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, msg));
							RoundTrip(msg);
							ExpectTruncatedFails(msg);
						}
						{
							const auto legacy = LegacyPacket([&] { LegacyChat::SendTeamStatus(receiver, 0x42, zone, byte, byte, name); });
							ClientPackets::TeamGetStatusResponse msg;
							msg.target = receiver.playerID;
							msg.i64LeaderID = 0x42;
							msg.i64LeaderZoneID = zone;
							msg.ucLootFlag = byte;
							msg.ucNumOfOtherPlayers = byte;
							msg.wsLeaderName = name;
							EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, msg));
							RoundTrip(msg);
							ExpectTruncatedFails(msg);
						}
					}
					for (const bool local : { false, true }) {
						for (const bool noLoot : { false, true }) {
							const auto legacy = LegacyPacket([&] { LegacyChat::SendTeamAddPlayer(receiver, flag, local, noLoot, 0x42, name, zone); });
							// As TeamContainer::SendTeamAddPlayer fills it in
							ClientPackets::TeamAddPlayer msg;
							msg.target = receiver.playerID;
							msg.bIsFreeTrial = flag;
							msg.bLocal = local;
							msg.bNoLootOnDeath = noLoot;
							msg.i64PlayerID = 0x42;
							msg.wsPlayerName = name;
							msg.zoneID = receiver.zoneID.GetCloneID() == zone.GetCloneID() ? LWOZONEID(zone.GetMapID(), zone.GetInstanceID(), 0) : zone;
							EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, msg));
							RoundTrip(msg);
							ExpectTruncatedFails(msg);

							for (const bool leaving : { false, true }) {
								const auto legacyRemove = LegacyPacket([&] { LegacyChat::SendTeamRemovePlayer(receiver, flag, noLoot, leaving, local, 0x99, 0x42, name); });
								ClientPackets::TeamRemovePlayer remove;
								remove.target = receiver.playerID;
								remove.bDisband = flag;
								remove.bIsKicked = noLoot;
								remove.bIsLeaving = leaving;
								remove.bLocal = local;
								remove.i64LeaderID = 0x99;
								remove.i64PlayerID = 0x42;
								remove.wsPlayerName = name;
								EXPECT_PACKET_EQ(FromCapture(legacyRemove), Routed(receiver.playerID, remove));
								RoundTrip(remove);
								ExpectTruncatedFails(remove);
							}
						}
					}
				}
			}
			for (const auto playerId : g_Ids) {
				const auto legacy = LegacyPacket([&] { LegacyChat::SendTeamSetLeader(receiver, playerId); });
				ClientPackets::TeamSetLeader msg;
				msg.target = receiver.playerID;
				msg.i64PlayerID = playerId;
				EXPECT_PACKET_EQ(FromCapture(legacy), Routed(receiver.playerID, msg));
				RoundTrip(msg);
				ExpectTruncatedFails(msg);

				const auto legacyFlag = LegacyPacket([&] { LegacyChat::SendTeamSetOffWorldFlag(receiver, playerId, zone); });
				// As TeamContainer::SendTeamSetOffWorldFlag fills it in
				ClientPackets::TeamSetOffWorldFlag flagMsg;
				flagMsg.target = receiver.playerID;
				flagMsg.i64PlayerID = playerId;
				flagMsg.zoneID = receiver.zoneID.GetCloneID() == zone.GetCloneID() ? LWOZONEID(zone.GetMapID(), zone.GetInstanceID(), 0) : zone;
				EXPECT_PACKET_EQ(FromCapture(legacyFlag), Routed(receiver.playerID, flagMsg));
				RoundTrip(flagMsg);
				ExpectTruncatedFails(flagMsg);
			}
		}
	}
}

TEST_F(ChatPacketsTests, TeamUpdateMatchesLegacyAndWorldReader) {
	for (const bool deleteTeam : { false, true }) {
		for (size_t count = 0; count <= 4; count++) {
			const std::vector<LWOOBJID> members(g_Ids.begin(), g_Ids.begin() + std::min<size_t>(count, g_Ids.size()));
			const auto legacy = LegacyPacket([&] { LegacyChat::UpdateTeamsOnWorld(0x1234, 1, members, deleteTeam); });
			ChatPackets::TeamUpdate update;
			update.teamID = 0x1234;
			update.deleteTeam = deleteTeam;
			if (!deleteTeam) {
				update.lootFlag = 1;
				update.members = members;
			}
			const auto sent = LegacyPacket([&] { update.Broadcast(); });
			EXPECT_PACKET_EQ(FromCapture(legacy), FromCapture(sent));
			EXPECT_TRUE(sent.broadcast);

			RakNet::BitStream stream; LoadPayload(stream, update);
			const auto read = LegacyChat::ReadTeamStatus(stream);
			const auto copy = RoundTrip(update);
			EXPECT_EQ(read.teamID, copy.teamID);
			EXPECT_EQ(read.deleteTeam, copy.deleteTeam);
			EXPECT_EQ(static_cast<uint8_t>(read.lootOption), copy.lootFlag);
			EXPECT_EQ(read.members, copy.members);
			ExpectTruncatedFails(update);
		}
	}
}

TEST_F(ChatPacketsTests, GMMuteMatchesLegacy) {
	for (const auto id : g_Ids) {
		for (const time_t expire : { static_cast<time_t>(0), static_cast<time_t>(1), static_cast<time_t>(1893456000), std::numeric_limits<time_t>::max() }) {
			const auto legacy = LegacyPacket([&] { LegacyChat::BroadcastMuteUpdate(id, expire); });
			ChatPackets::GMMute mute;
			mute.playerID = id;
			mute.expire = expire;
			const auto sent = LegacyPacket([&] { mute.Broadcast(); });
			EXPECT_PACKET_EQ(FromCapture(legacy), FromCapture(sent));
			EXPECT_TRUE(sent.broadcast);
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyChat::WriteGMMute(b, id, expire); }), StructPacket(mute));
			RoundTrip(mute);
			ExpectTruncatedFails(mute);
		}
	}
	ChatPackets::GMMute mute;
	mute.playerID = 0x0102030405060708LL;
	mute.expire = 1;
	EXPECT_PACKET_EQ(FromHex("53 02 00 2e 00 00 00 00 08 07 06 05 04 03 02 01 01 00 00 00 00 00 00 00"), StructPacket(mute));
}

// World -> chat

TEST_F(ChatPacketsTests, WorldToChatMatchesLegacy) {
	for (const auto id : g_Ids) {
		for (const auto& name : g_Names) {
			for (const auto& zone : g_Zones) {
				for (const auto gmLevel : { eGameMasterLevel::CIVILIAN, eGameMasterLevel::OPERATOR }) {
					ChatPackets::LoginSessionNotify notify;
					notify.playerID = id;
					notify.playerName = name;
					notify.zoneID = zone;
					notify.muteExpire = 1893456000;
					notify.gmLevel = gmLevel;
					EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyChat::WriteLoginSessionNotify(b, id, name, zone, 1893456000, gmLevel); }), StructPacket(notify));
					const auto copy = RoundTrip(notify);
					EXPECT_EQ(copy.playerName, name);
					ExpectTruncatedFails(notify);

					// What PlayerContainer::InsertPlayer read
					RakNet::BitStream stream; LoadPayload(stream, notify);
					PlayerData data;
					ASSERT_TRUE(LegacyChat::ReadLoginSessionNotify(stream, data));
					EXPECT_EQ(data.playerID, copy.playerID);
					EXPECT_EQ(data.playerName, copy.playerName);
					EXPECT_EQ(data.zoneID, copy.zoneID);
					EXPECT_EQ(data.muteExpire, copy.muteExpire);
					EXPECT_EQ(data.gmLevel, copy.gmLevel);

					ChatPackets::GMLevelUpdate update;
					update.playerID = id;
					update.gmLevel = gmLevel;
					EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyChat::WriteGMLevelUpdate(b, id, gmLevel); }), StructPacket(update));
					RoundTrip(update);
				}
			}
		}

		ChatPackets::UnexpectedDisconnect disconnect;
		disconnect.playerID = id;
		EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyChat::WriteUnexpectedDisconnect(b, id); }), StructPacket(disconnect));
		RoundTrip(disconnect);
		ExpectTruncatedFails(disconnect);
	}

	// Names longer than 33 characters were refused
	ChatPackets::LoginSessionNotify tooLong;
	tooLong.playerName = std::string(34, 'x');
	RakNet::BitStream stream; LoadPayload(stream, tooLong);
	ChatPackets::LoginSessionNotify read;
	EXPECT_FALSE(read.Deserialize(stream));
}

// Live updates: players sent again to a new chat server carry a resync byte; every other notify stays as it was
TEST_F(ChatPacketsTests, LoginSessionNotifyResync) {
	ChatPackets::LoginSessionNotify plain;
	plain.playerID = 0x42;
	plain.playerName = "Name";
	ChatPackets::LoginSessionNotify resync = plain;
	resync.resync = true;
	RakNet::BitStream plainStream; plain.Serialize(plainStream);
	RakNet::BitStream resyncStream; resync.Serialize(resyncStream);
	EXPECT_EQ(resyncStream.GetNumberOfBitsUsed(), plainStream.GetNumberOfBitsUsed() + 8);

	ChatPackets::LoginSessionNotify readPlain, readResync;
	ASSERT_TRUE(readPlain.Deserialize(plainStream));
	ASSERT_TRUE(readResync.Deserialize(resyncStream));
	EXPECT_FALSE(readPlain.resync);
	EXPECT_TRUE(readResync.resync);
	EXPECT_EQ(readResync.playerName, "Name");
}

TEST_F(ChatPacketsTests, CreateTeamMatchesLegacy) {
	for (size_t count = 0; count <= 3; count++) {
		for (const auto& zone : g_Zones) {
			const std::vector<LWOOBJID> members(g_Ids.begin(), g_Ids.begin() + count);
			ChatPackets::CreateTeam createTeam;
			createTeam.leaderID = 0x42;
			createTeam.members = members;
			createTeam.zoneID = zone;
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyChat::WriteCreateTeam(b, 0x42, members, zone); }), StructPacket(createTeam));

			RakNet::BitStream stream; LoadPayload(stream, createTeam);
			const auto read = LegacyChat::ReadCreateTeam(stream);
			const auto copy = RoundTrip(createTeam);
			EXPECT_EQ(read.playerID, copy.leaderID);
			EXPECT_EQ(read.members, copy.members);
			EXPECT_EQ(read.zoneId, copy.zoneID);
			ExpectTruncatedFails(createTeam);
		}
	}
	// 4 or more players can't be a team (CreateTeamServer refused them)
	ChatPackets::CreateTeam tooBig;
	tooBig.members = { 1, 2, 3, 4 };
	RakNet::BitStream stream; LoadPayload(stream, tooBig);
	ChatPackets::CreateTeam read;
	EXPECT_FALSE(read.Deserialize(stream));
}

TEST_F(ChatPacketsTests, AnnouncementMatchesLegacy) {
	for (const auto& title : g_Names) {
		for (const auto& message : g_Names) {
			ChatPackets::Announcement announcement;
			announcement.title = title;
			announcement.message = message;
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyChat::WriteGMAnnounce(b, title, message); }), StructPacket(announcement));
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyChat::WriteAnnouncement(b, title, message); }), StructPacket(announcement));

			RakNet::BitStream stream; LoadPayload(stream, announcement);
			const auto [legacyTitle, legacyMessage] = LegacyChat::ReadGMAnnounce(stream);
			const auto copy = RoundTrip(announcement);
			EXPECT_EQ(legacyTitle, copy.title);
			EXPECT_EQ(legacyMessage, copy.message);
			ExpectTruncatedFails(announcement);
		}
	}
}

TEST_F(ChatPacketsTests, ShowAllAndWhoRequestsMatchLegacy) {
	for (const auto id : g_Ids) {
		for (const bool zoneData : { false, true }) {
			for (const bool individualPlayers : { false, true }) {
				LegacyChat::ShowAllRequest legacy{ id, zoneData, individualPlayers };
				ChatPackets::ShowAllRequest request;
				request.requestor = id;
				request.displayZoneData = zoneData;
				request.displayIndividualPlayers = individualPlayers;
				EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { legacy.Serialize(b); }), StructPacket(request));

				RakNet::BitStream stream; LoadPayload(stream, request);
				LegacyChat::ShowAllRequest legacyRead;
				legacyRead.Deserialize(stream);
				const auto copy = RoundTrip(request);
				EXPECT_EQ(legacyRead.requestor, copy.requestor);
				EXPECT_EQ(legacyRead.displayZoneData, copy.displayZoneData);
				EXPECT_EQ(legacyRead.displayIndividualPlayers, copy.displayIndividualPlayers);
				ExpectTruncatedFails(request);
			}
		}
		for (const auto& name : g_Names) {
			LegacyChat::FindPlayerRequest legacy{ id, LUWString(name) };
			ChatPackets::FindPlayerRequest request;
			request.requestor = id;
			request.playerName = LUWString(name);
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { legacy.Serialize(b); }), StructPacket(request));

			RakNet::BitStream stream; LoadPayload(stream, request);
			LegacyChat::FindPlayerRequest legacyRead;
			legacyRead.Deserialize(stream);
			const auto copy = RoundTrip(request);
			EXPECT_EQ(legacyRead.playerName.string, copy.playerName.string);
			ExpectTruncatedFails(request);
		}
	}
}

// Client -> world -> chat: the structs read what the old handlers read

TEST_F(ChatPacketsTests, FriendAndIgnoreRequestsReadLikeLegacy) {
	for (const auto id : g_Ids) {
		for (const auto& name : g_Names) {
			for (const uint8_t extra : { 0, 1, 3 }) {
				ChatPackets::AddFriendRequest request;
				request.playerID = id;
				request.unknown = 0xAABBCCDD;
				request.friendName = LUWString(name);
				request.isBestFriendRequest = extra;
				RakNet::BitStream stream; LoadPayload(stream, request);
				const auto legacy = LegacyChat::ReadFriendRequest(stream);
				const auto copy = RoundTrip(request);
				EXPECT_EQ(legacy.playerID, copy.playerID);
				EXPECT_EQ(legacy.name.string, copy.friendName.string);
				EXPECT_EQ(static_cast<uint8_t>(legacy.extra), copy.isBestFriendRequest);
				ExpectTruncatedFails(request);

				ChatPackets::AddFriendResponse response;
				response.playerID = id;
				response.responseCode = static_cast<eAddFriendResponseCode>(extra);
				response.friendName = LUWString(name);
				RakNet::BitStream responseStream; LoadPayload(responseStream, response);
				const auto legacyResponse = LegacyChat::ReadFriendResponse(responseStream);
				const auto responseCopy = RoundTrip(response);
				EXPECT_EQ(legacyResponse.playerID, responseCopy.playerID);
				EXPECT_EQ(legacyResponse.name.string, responseCopy.friendName.string);
				EXPECT_EQ(static_cast<uint8_t>(legacyResponse.extra), static_cast<uint8_t>(responseCopy.responseCode));
				ExpectTruncatedFails(response);
			}

			const auto checkPlayerAndName = [&](const auto& msg, const LUWString& name) {
				RakNet::BitStream stream; LoadPayload(stream, msg);
				const auto legacy = LegacyChat::ReadPlayerAndName(stream);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(legacy.playerID, copy.playerID);
				EXPECT_EQ(legacy.name.string, name.string);
				ExpectTruncatedFails(msg);
			};
			ChatPackets::RemoveFriend removeFriend;
			removeFriend.playerID = id;
			removeFriend.friendName = LUWString(name);
			checkPlayerAndName(removeFriend, removeFriend.friendName);
			ChatPackets::AddIgnore addIgnore;
			addIgnore.playerID = id;
			addIgnore.playerName = LUWString(name);
			checkPlayerAndName(addIgnore, addIgnore.playerName);
			ChatPackets::RemoveIgnore removeIgnore;
			removeIgnore.playerID = id;
			removeIgnore.playerName = LUWString(name);
			checkPlayerAndName(removeIgnore, removeIgnore.playerName);
			ChatPackets::TeamInvite teamInvite;
			teamInvite.playerID = id;
			teamInvite.invitedPlayer = LUWString(name);
			checkPlayerAndName(teamInvite, teamInvite.invitedPlayer);
			ChatPackets::TeamKick teamKick;
			teamKick.playerID = id;
			teamKick.kickedPlayer = LUWString(name);
			checkPlayerAndName(teamKick, teamKick.kickedPlayer);
			ChatPackets::TeamSetLeader teamSetLeader;
			teamSetLeader.playerID = id;
			teamSetLeader.promotedPlayer = LUWString(name);
			checkPlayerAndName(teamSetLeader, teamSetLeader.promotedPlayer);
		}

		ChatPackets::TeamInviteResponse inviteResponse;
		inviteResponse.playerID = id;
		inviteResponse.declined = 1;
		inviteResponse.leaderID = 0x1234;
		RakNet::BitStream stream; LoadPayload(stream, inviteResponse);
		const auto legacy = LegacyChat::ReadTeamInviteResponse(stream);
		EXPECT_EQ(legacy.playerID, id);
		EXPECT_EQ(legacy.declined, 1);
		EXPECT_EQ(legacy.leaderID, 0x1234);
		RoundTrip(inviteResponse);
		ExpectTruncatedFails(inviteResponse);

		ChatPackets::TeamSetLoot setLoot;
		setLoot.playerID = id;
		setLoot.lootFlag = 1;
		RakNet::BitStream lootStream; LoadPayload(lootStream, setLoot);
		const auto [lootPlayer, lootOption] = LegacyChat::ReadTeamLootOption(lootStream);
		EXPECT_EQ(lootPlayer, id);
		EXPECT_EQ(lootOption, 1);
		RoundTrip(setLoot);
		ExpectTruncatedFails(setLoot);

		for (const auto& msg : std::initializer_list<std::function<void()>>{
			[&] { ChatPackets::GetFriendsList m; m.playerID = id; RoundTrip(m); ExpectTruncatedFails(m); },
			[&] { ChatPackets::GetIgnoreList m; m.playerID = id; RoundTrip(m); ExpectTruncatedFails(m); },
			[&] { ChatPackets::TeamGetStatus m; m.playerID = id; RoundTrip(m); ExpectTruncatedFails(m); },
			[&] { ChatPackets::TeamLeave m; m.playerID = id; RoundTrip(m); ExpectTruncatedFails(m); },
		}) msg();
	}
}

TEST_F(ChatPacketsTests, ChatMessagesReadLikeLegacy) {
	for (const auto channel : { eChatChannel::TEAM, eChatChannel::PRIVATE_CHAT, eChatChannel::LOCAL }) {
		for (const uint32_t size : { 0u, 1u, 6u, 50u }) {
			ChatPackets::GeneralChatMessage chatMessage;
			chatMessage.playerID = 0x0102030405060708LL;
			chatMessage.chatChannel = channel;
			chatMessage.messageLength = size;
			chatMessage.senderName = LUWString("Sender");
			chatMessage.senderID = 0x42;
			chatMessage.message = LUWString(u"hello", size);
			RakNet::BitStream stream; LoadPayload(stream, chatMessage);
			const auto legacy = LegacyChat::ReadChatMessage(stream);
			const auto copy = RoundTrip(chatMessage);
			EXPECT_EQ(legacy.playerID, copy.playerID);
			EXPECT_EQ(legacy.channel, copy.chatChannel);
			EXPECT_EQ(legacy.size, copy.messageLength);
			EXPECT_EQ(legacy.message.string, copy.message.string);
			ExpectTruncatedFails(chatMessage);

			ChatPackets::PrivateChatMessage privateMessage;
			privateMessage.playerID = 0x0102030405060708LL;
			privateMessage.chatChannel = channel;
			privateMessage.messageLength = size;
			privateMessage.receiverName = LUWString("Receiver");
			privateMessage.receiverGMLevel = 3;
			privateMessage.responseCode = 4;
			privateMessage.message = LUWString(u"hello", size);
			RakNet::BitStream privateStream; LoadPayload(privateStream, privateMessage);
			const auto legacyPrivate = LegacyChat::ReadPrivateChatMessage(privateStream);
			const auto privateCopy = RoundTrip(privateMessage);
			EXPECT_EQ(legacyPrivate.playerID, privateCopy.playerID);
			EXPECT_EQ(legacyPrivate.channel, privateCopy.chatChannel);
			EXPECT_EQ(legacyPrivate.receiverName.string, privateCopy.receiverName.string);
			EXPECT_EQ(legacyPrivate.message.string, privateCopy.message.string);
			ExpectTruncatedFails(privateMessage);
		}
	}

	// Longer than MAX_MESSAGE_LENGTH: the old handlers ignored the message, the structs refuse it
	ChatPackets::GeneralChatMessage tooLong;
	tooLong.messageLength = MAX_MESSAGE_LENGTH + 1;
	RakNet::BitStream stream; LoadPayload(stream, tooLong);
	ChatPackets::GeneralChatMessage read;
	EXPECT_FALSE(read.Deserialize(stream));
}

TEST_F(ChatPacketsTests, AchievementNotifyRoundTrips) {
	ChatPackets::AchievementNotify notify;
	notify.targetPlayerName = LUWString("Friend");
	notify.missionEmailID = 1234;
	notify.earningPlayerID = 0x0102030405060708LL;
	notify.earnerName = LUWString("Earner");
	const auto copy = RoundTrip(notify);
	EXPECT_EQ(copy.missionEmailID, 1234u);
	EXPECT_EQ(copy.earnerName.string, u"Earner");
	EXPECT_EQ(copy.targetPlayerName.string, u"Friend");
	ExpectTruncatedFails(notify);
}

// DLU's own world -> chat -> world new-mail notice: header (chat service, MAIL) and the receiver's object ID.
TEST_F(ChatPacketsTests, MailNotifyRoundTrips) {
	ChatPackets::MailNotify notify;
	notify.receiverID = 0x1000000000000042LL;
	EXPECT_PACKET_EQ(FromHex("53 02 00 24 00 00 00 00 42 00 00 00 00 00 00 10"), StructPacket(notify));
	EXPECT_EQ(RoundTrip(notify).receiverID, notify.receiverID);
	ExpectTruncatedFails(notify);
}
