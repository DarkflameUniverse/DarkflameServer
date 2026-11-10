#include <gtest/gtest.h>

#include "ChatRouting.h"

TEST(ChatRoutingTest, PlayersRouteTheMessagesTheClientSends) {
	const auto civilian = eGameMasterLevel::CIVILIAN;
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::GENERAL_CHAT_MESSAGE, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::PRIVATE_CHAT_MESSAGE, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::ADD_FRIEND_REQUEST, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::ADD_FRIEND_RESPONSE, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::REMOVE_FRIEND, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::GET_FRIENDS_LIST, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::ADD_IGNORE, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::REMOVE_IGNORE, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::GET_IGNORE_LIST, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::TEAM_INVITE, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::TEAM_INVITE_RESPONSE, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::TEAM_KICK, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::TEAM_LEAVE, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::TEAM_SET_LOOT, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::TEAM_SET_LEADER, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::TEAM_GET_STATUS, civilian));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::WHO, civilian));
}

TEST(ChatRoutingTest, ServerMessagesAreNeverRouted) {
	for (const auto level : { eGameMasterLevel::CIVILIAN, eGameMasterLevel::OPERATOR }) {
		EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::LOGIN_SESSION_NOTIFY, level));
		EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::GMLEVEL_UPDATE, level));
		EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::GM_KICK, level));
		EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::GM_ANNOUNCE, level));
		EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::GM_MUTE, level));
		EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::WORLD_ROUTE_PACKET, level));
		EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::ACHIEVEMENT_NOTIFY, level));
		EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::UNEXPECTED_DISCONNECT, level));
		EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::CREATE_TEAM, level));
		// Not a message at all
		EXPECT_FALSE(ChatRouting::CanRoute(static_cast<MessageType::Chat>(0x100 | 1), level));
	}
}

TEST(ChatRoutingTest, ShowAllNeedsTheChatCommandLevel) {
	EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::SHOW_ALL, eGameMasterLevel::CIVILIAN));
	EXPECT_FALSE(ChatRouting::CanRoute(MessageType::Chat::SHOW_ALL, eGameMasterLevel::FORUM_MODERATOR));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::SHOW_ALL, eGameMasterLevel::JUNIOR_MODERATOR));
	EXPECT_TRUE(ChatRouting::CanRoute(MessageType::Chat::SHOW_ALL, eGameMasterLevel::OPERATOR));
}
