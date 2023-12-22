#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include "StringifiedEnum.h"
#include "Logger.h"
#include "Game.h"
#include "eGameMessageType.h"
#include "eWorldMessageType.h"

#define ENUM_EQ(e, y, z)\
	LOG("%s %s", StringifiedEnum::ToString(static_cast<e>(y)).data(), #z);\
	ASSERT_STREQ(StringifiedEnum::ToString(static_cast<e>(y)).data(), #z);

#define ENUM_NE(e, y)\
	ENUM_EQ(e, y, UNKNOWN);

// Test World Message Enum Reflection
TEST(MagicEnumTest, eWorldMessageTypeTest) {

	Game::logger = new Logger("./MagicEnumTest_WM.log", true, true);
	
	ENUM_EQ(eWorldMessageType, 1, VALIDATION);
	ENUM_EQ(eWorldMessageType, 2, CHARACTER_LIST_REQUEST);
	ENUM_EQ(eWorldMessageType, 3, CHARACTER_CREATE_REQUEST);
	ENUM_EQ(eWorldMessageType, 4, LOGIN_REQUEST);
	ENUM_EQ(eWorldMessageType, 5, GAME_MSG);
	ENUM_EQ(eWorldMessageType, 6, CHARACTER_DELETE_REQUEST);
	ENUM_EQ(eWorldMessageType, 7, CHARACTER_RENAME_REQUEST);
	ENUM_EQ(eWorldMessageType, 8, HAPPY_FLOWER_MODE_NOTIFY);
	ENUM_EQ(eWorldMessageType, 9, SLASH_RELOAD_MAP);
	ENUM_EQ(eWorldMessageType, 10, SLASH_PUSH_MAP_REQUEST);
	ENUM_EQ(eWorldMessageType, 11, SLASH_PUSH_MAP);
	ENUM_EQ(eWorldMessageType, 12, SLASH_PULL_MAP);
	ENUM_EQ(eWorldMessageType, 13, LOCK_MAP_REQUEST);
	ENUM_EQ(eWorldMessageType, 14, GENERAL_CHAT_MESSAGE);
	ENUM_EQ(eWorldMessageType, 15, HTTP_MONITOR_INFO_REQUEST);
	ENUM_EQ(eWorldMessageType, 16, SLASH_DEBUG_SCRIPTS);
	ENUM_EQ(eWorldMessageType, 17, MODELS_CLEAR);
	ENUM_EQ(eWorldMessageType, 18, EXHIBIT_INSERT_MODEL);
	ENUM_EQ(eWorldMessageType, 19, LEVEL_LOAD_COMPLETE);
	ENUM_EQ(eWorldMessageType, 20, TMP_GUILD_CREATE);
	ENUM_EQ(eWorldMessageType, 21, ROUTE_PACKET);
	ENUM_EQ(eWorldMessageType, 22, POSITION_UPDATE);
	ENUM_EQ(eWorldMessageType, 23, MAIL);
	ENUM_EQ(eWorldMessageType, 24, WORD_CHECK);
	ENUM_EQ(eWorldMessageType, 25, STRING_CHECK);
	ENUM_EQ(eWorldMessageType, 26, GET_PLAYERS_IN_ZONE);
	ENUM_EQ(eWorldMessageType, 27, REQUEST_UGC_MANIFEST_INFO);
	ENUM_EQ(eWorldMessageType, 28, BLUEPRINT_GET_ALL_DATA_REQUEST);
	ENUM_EQ(eWorldMessageType, 29, CANCEL_MAP_QUEUE);
	ENUM_EQ(eWorldMessageType, 30, HANDLE_FUNNESS);
	ENUM_EQ(eWorldMessageType, 31, FAKE_PRG_CSR_MESSAGE);
	ENUM_EQ(eWorldMessageType, 32, REQUEST_FREE_TRIAL_REFRESH);
	ENUM_EQ(eWorldMessageType, 33, GM_SET_FREE_TRIAL_STATUS);
	ENUM_EQ(eWorldMessageType, 91, UI_HELP_TOP_5);
	ENUM_NE(eWorldMessageType, 37);
	ENUM_NE(eWorldMessageType, 123);

	srand(time(NULL));
	auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 10000000; ++i) {
		volatile auto f = StringifiedEnum::ToString(static_cast<eWorldMessageType>(i)).data();

		// To ensure the compiler doesn't optimize out the call, I print it at random intervals
		if (rand() % 100000 == 0) LOG("%i, %s", i, f);
	}
	auto end = std::chrono::high_resolution_clock::now();
	LOG("Time: %lld", std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());

	delete Game::logger;
}

// Test Game Message Enum Reflection
TEST(MagicEnumTest, eGameMessageTypeTest) {
	
	Game::logger = new Logger("./MagicEnumTest_GM.log", true, true);
	
	// Only doing the first and last 10 for the sake of my sanity
	ENUM_EQ(eGameMessageType, 0, GET_POSITION);
	ENUM_EQ(eGameMessageType, 1, GET_ROTATION);
	ENUM_EQ(eGameMessageType, 2, GET_LINEAR_VELOCITY);
	ENUM_EQ(eGameMessageType, 3, GET_ANGULAR_VELOCITY);
	ENUM_EQ(eGameMessageType, 4, GET_FORWARD_VELOCITY);
	ENUM_EQ(eGameMessageType, 5, GET_PLAYER_FORWARD);
	ENUM_EQ(eGameMessageType, 6, GET_FORWARD_VECTOR);
	ENUM_EQ(eGameMessageType, 7, SET_POSITION);
	ENUM_EQ(eGameMessageType, 8, SET_LOCAL_POSITION);
	ENUM_EQ(eGameMessageType, 9, SET_ROTATION);
	ENUM_EQ(eGameMessageType, 10, SET_LINEAR_VELOCITY);
	ENUM_EQ(eGameMessageType, 1762, USE_SKILL_SET);
	ENUM_EQ(eGameMessageType, 1763, SET_SKILL_SET_POSSESSOR);
	ENUM_EQ(eGameMessageType, 1764, POPULATE_ACTION_BAR);
	ENUM_EQ(eGameMessageType, 1765, GET_COMPONENT_TEMPLATE_ID);
	ENUM_EQ(eGameMessageType, 1766, GET_POSSESSABLE_SKILL_SET);
	ENUM_EQ(eGameMessageType, 1767, MARK_INVENTORY_ITEM_AS_ACTIVE);
	ENUM_EQ(eGameMessageType, 1768, UPDATE_FORGED_ITEM);
	ENUM_EQ(eGameMessageType, 1769, CAN_ITEMS_BE_REFORGED);
	ENUM_EQ(eGameMessageType, 1771, NOTIFY_CLIENT_RAIL_START_FAILED);
	ENUM_EQ(eGameMessageType, 1772, GET_IS_ON_RAIL);
	ENUM_NE(eGameMessageType, 32);
	ENUM_NE(eGameMessageType, 1776);

	srand(time(NULL));
	auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 10000000; ++i) {
		volatile auto f = StringifiedEnum::ToString(static_cast<eGameMessageType>(i)).data();

		// To ensure the compiler doesn't optimize out the call, I print it at random intervals
		if (rand() % 100000 == 0) LOG("%i, %s", i, f);
	}
	auto end = std::chrono::high_resolution_clock::now();
	LOG("Time: %lld", std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());

	delete Game::logger;
}
