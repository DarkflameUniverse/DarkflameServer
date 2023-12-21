#include <gtest/gtest.h>

#include "StringifiedEnum.h"
#include "Logger.h"
#include "Game.h"
#include "eGameMessageType.h"
#include "eWorldMessageType.h"
#include <chrono>
#include <string>

#define log_test(e, y, z)\
	LOG("%s %s", StringifiedEnum::ToString(static_cast<e>(y)).data(), #z);\
	ASSERT_STREQ(StringifiedEnum::ToString(static_cast<e>(y)).data(), #z);

#define log_test_invalid(e, y)\
	log_test(e, y, "UNKNOWN");

// Test World Message Enum Reflection
TEST(MagicEnumTest, eWorldMessageTypeTest) {

	Game::logger = new Logger("./MagicEnumTest.log", true, true);
	
	log_test(eWorldMessageType, 1, VALIDATION);
	log_test(eWorldMessageType, 2, CHARACTER_LIST_REQUEST);
	log_test(eWorldMessageType, 3, CHARACTER_CREATE_REQUEST);
	log_test(eWorldMessageType, 4, LOGIN_REQUEST);
	log_test(eWorldMessageType, 5, GAME_MSG);
	log_test(eWorldMessageType, 6, CHARACTER_DELETE_REQUEST);
	log_test(eWorldMessageType, 7, CHARACTER_RENAME_REQUEST);
	log_test(eWorldMessageType, 8, HAPPY_FLOWER_MODE_NOTIFY);
	log_test(eWorldMessageType, 9, SLASH_RELOAD_MAP);
	log_test(eWorldMessageType, 10, SLASH_PUSH_MAP_REQUEST);
	log_test(eWorldMessageType, 11, SLASH_PUSH_MAP);
	log_test(eWorldMessageType, 12, SLASH_PULL_MAP);
	log_test(eWorldMessageType, 13, LOCK_MAP_REQUEST);
	log_test(eWorldMessageType, 14, GENERAL_CHAT_MESSAGE);
	log_test(eWorldMessageType, 15, HTTP_MONITOR_INFO_REQUEST);
	log_test(eWorldMessageType, 16, SLASH_DEBUG_SCRIPTS);
	log_test(eWorldMessageType, 17, MODELS_CLEAR);
	log_test(eWorldMessageType, 18, EXHIBIT_INSERT_MODEL);
	log_test(eWorldMessageType, 19, LEVEL_LOAD_COMPLETE);
	log_test(eWorldMessageType, 20, TMP_GUILD_CREATE);
	log_test(eWorldMessageType, 21, ROUTE_PACKET);
	log_test(eWorldMessageType, 22, POSITION_UPDATE);
	log_test(eWorldMessageType, 23, MAIL);
	log_test(eWorldMessageType, 24, WORD_CHECK);
	log_test(eWorldMessageType, 25, STRING_CHECK);
	log_test(eWorldMessageType, 26, GET_PLAYERS_IN_ZONE);
	log_test(eWorldMessageType, 27, REQUEST_UGC_MANIFEST_INFO);
	log_test(eWorldMessageType, 28, BLUEPRINT_GET_ALL_DATA_REQUEST);
	log_test(eWorldMessageType, 29, CANCEL_MAP_QUEUE);
	log_test(eWorldMessageType, 30, HANDLE_FUNNESS);
	log_test(eWorldMessageType, 31, FAKE_PRG_CSR_MESSAGE);
	log_test(eWorldMessageType, 32, REQUEST_FREE_TRIAL_REFRESH);
	log_test(eWorldMessageType, 33, GM_SET_FREE_TRIAL_STATUS);
	log_test(eWorldMessageType, 91, UI_HELP_TOP_5);
	log_test_invalid(eWorldMessageType, 37);
	log_test_invalid(eWorldMessageType, 123);

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
	
	Game::logger = new Logger("./MagicEnumTest.log", true, true);
	
	// Only doing the first and last 10 for the sake of my sanity
	log_test(eGameMessageType, 0, GET_POSITION);
	log_test(eGameMessageType, 1, GET_ROTATION);
	log_test(eGameMessageType, 2, GET_LINEAR_VELOCITY);
	log_test(eGameMessageType, 3, GET_ANGULAR_VELOCITY);
	log_test(eGameMessageType, 4, GET_FORWARD_VELOCITY);
	log_test(eGameMessageType, 5, GET_PLAYER_FORWARD);
	log_test(eGameMessageType, 6, GET_FORWARD_VECTOR);
	log_test(eGameMessageType, 7, SET_POSITION);
	log_test(eGameMessageType, 8, SET_LOCAL_POSITION);
	log_test(eGameMessageType, 9, SET_ROTATION);
	log_test(eGameMessageType, 10, SET_LINEAR_VELOCITY);
	log_test(eGameMessageType, 1762, USE_SKILL_SET);
	log_test(eGameMessageType, 1763, SET_SKILL_SET_POSSESSOR);
	log_test(eGameMessageType, 1764, POPULATE_ACTION_BAR);
	log_test(eGameMessageType, 1765, GET_COMPONENT_TEMPLATE_ID);
	log_test(eGameMessageType, 1766, GET_POSSESSABLE_SKILL_SET);
	log_test(eGameMessageType, 1767, MARK_INVENTORY_ITEM_AS_ACTIVE);
	log_test(eGameMessageType, 1768, UPDATE_FORGED_ITEM);
	log_test(eGameMessageType, 1769, CAN_ITEMS_BE_REFORGED);
	log_test(eGameMessageType, 1771, NOTIFY_CLIENT_RAIL_START_FAILED);
	log_test(eGameMessageType, 1772, GET_IS_ON_RAIL);
	log_test_invalid(eGameMessageType, 32);
	log_test_invalid(eGameMessageType, 1776);

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
