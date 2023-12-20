#include <gtest/gtest.h>

#include "magic_enum.hpp"
#include "Logger.h"
#include "Game.h"
#include "eWorldMessageType.h"
#include <chrono>

#define log_test(y, z)\
	LOG("%s %s", magic_enum::enum_name(static_cast<eWorldMessageType>(y)).data(), #z);\
	ASSERT_STREQ(magic_enum::enum_name(static_cast<eWorldMessageType>(y)).data(), #z);

#define log_test_invalid(y)\
	LOG("%s", magic_enum::enum_name(static_cast<eWorldMessageType>(y)).data());\
	ASSERT_STREQ(magic_enum::enum_name(static_cast<eWorldMessageType>(y)).data(), NULL);

TEST(MagicEnumTest, eWorldMessageTypeTest) {
	Game::logger = new Logger("./MagicEnumTest.log", true, true);
	
	log_test(1, VALIDATION);
	log_test(2, CHARACTER_LIST_REQUEST);
	log_test(3, CHARACTER_CREATE_REQUEST);
	log_test(4, LOGIN_REQUEST);
	log_test(5, GAME_MSG);
	log_test(6, CHARACTER_DELETE_REQUEST);
	log_test(7, CHARACTER_RENAME_REQUEST);
	log_test(8, HAPPY_FLOWER_MODE_NOTIFY);
	log_test(9, SLASH_RELOAD_MAP);
	log_test(10, SLASH_PUSH_MAP_REQUEST);
	log_test(11, SLASH_PUSH_MAP);
	log_test(12, SLASH_PULL_MAP);
	log_test(13, LOCK_MAP_REQUEST);
	log_test(14, GENERAL_CHAT_MESSAGE);
	log_test(15, HTTP_MONITOR_INFO_REQUEST);
	log_test(16, SLASH_DEBUG_SCRIPTS);
	log_test(17, MODELS_CLEAR);
	log_test(18, EXHIBIT_INSERT_MODEL);
	log_test(19, LEVEL_LOAD_COMPLETE);
	log_test(20, TMP_GUILD_CREATE);
	log_test(21, ROUTE_PACKET);
	log_test(22, POSITION_UPDATE);
	log_test(23, MAIL);
	log_test(24, WORD_CHECK);
	log_test(25, STRING_CHECK);
	log_test(26, GET_PLAYERS_IN_ZONE);
	log_test(27, REQUEST_UGC_MANIFEST_INFO);
	log_test(28, BLUEPRINT_GET_ALL_DATA_REQUEST);
	log_test(29, CANCEL_MAP_QUEUE);
	log_test(30, HANDLE_FUNNESS);
	log_test(31, FAKE_PRG_CSR_MESSAGE);
	log_test(32, REQUEST_FREE_TRIAL_REFRESH);
	log_test(33, GM_SET_FREE_TRIAL_STATUS);
	log_test(91, UI_HELP_TOP_5);
	log_test_invalid(123);

	auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 10000000; ++i) {
		volatile auto f = magic_enum::enum_name(static_cast<eWorldMessageType>(i)).data();

		// To ensure the compiler doesn't optimize out the call, I print it at random intervals
		if (rand() % 100000 == 0) LOG("%i, %s", i, f);
	}
	auto end = std::chrono::high_resolution_clock::now();
	LOG("Time: %lld", std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());

	delete Game::logger;
}
