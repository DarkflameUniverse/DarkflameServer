#include <gtest/gtest.h>

#include "Logger.h"
#include "Game.h"
#include "eWorldMessageType.h"
#include <chrono>

#define funny(y, z) LOG("%s %s", eWorldMessageType_as_string(static_cast<eWorldMessageType>(y)), #z); ASSERT_EQ(eWorldMessageType_as_string(static_cast<eWorldMessageType>(y)), #z);
#define funny_invalid(y) LOG("%s", eWorldMessageType_as_string(static_cast<eWorldMessageType>(y))); ASSERT_EQ(eWorldMessageType_as_string(static_cast<eWorldMessageType>(y)), "<none>");

TEST(StringifiedMacroTest, eWorldMessageTypeMacroTest) {
	Game::logger = new Logger("./MacroTest.log", true, true);
	funny(1, VALIDATION);
	funny(2, CHARACTER_LIST_REQUEST);
	funny(3, CHARACTER_CREATE_REQUEST);
	funny(4, LOGIN_REQUEST);
	funny(5, GAME_MSG);
	funny(6, CHARACTER_DELETE_REQUEST);
	funny(7, CHARACTER_RENAME_REQUEST);
	funny(8, HAPPY_FLOWER_MODE_NOTIFY);
	funny(9, SLASH_RELOAD_MAP);
	funny(10, SLASH_PUSH_MAP_REQUEST);
	funny(11, SLASH_PUSH_MAP);
	funny(12, SLASH_PULL_MAP);
	funny(13, LOCK_MAP_REQUEST);
	funny(14, GENERAL_CHAT_MESSAGE);
	funny(15, HTTP_MONITOR_INFO_REQUEST);
	funny(16, SLASH_DEBUG_SCRIPTS);
	funny(17, MODELS_CLEAR);
	funny(18, EXHIBIT_INSERT_MODEL);
	funny(19, LEVEL_LOAD_COMPLETE);
	funny(20, TMP_GUILD_CREATE);
	funny(21, ROUTE_PACKET);
	funny(22, POSITION_UPDATE);
	funny(23, MAIL);
	funny(24, WORD_CHECK);
	funny(25, STRING_CHECK);
	funny(26, GET_PLAYERS_IN_ZONE);
	funny(27, REQUEST_UGC_MANIFEST_INFO);
	funny(28, BLUEPRINT_GET_ALL_DATA_REQUEST);
	funny(29, CANCEL_MAP_QUEUE);
	funny(30, HANDLE_FUNNESS);
	funny(31, FAKE_PRG_CSR_MESSAGE);
	funny(32, REQUEST_FREE_TRIAL_REFRESH);
	funny(33, GM_SET_FREE_TRIAL_STATUS);
	funny(91, UI_HELP_TOP_5);
	funny_invalid(123);

	auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 10000000; ++i) {
		volatile auto f = eWorldMessageType_as_string(static_cast<eWorldMessageType>(i));

		// To ensure the compiler doesn't optimize out the call, I print it at random intervals
		if (rand() % 100000 == 0) LOG("%i, %s", i, f);
	}
	auto end = std::chrono::high_resolution_clock::now();
	LOG("Time: %lld", std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());

	delete Game::logger;
}
