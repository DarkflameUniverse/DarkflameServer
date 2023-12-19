#include <gtest/gtest.h>

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 2048

#include "magic_enum.hpp"
#include "Logger.h"
#include "Game.h"
#include "eWorldMessageType.h"
#include <chrono>

#define log_test(y, z) LOG("%s %s", magic_enum::enum_name<eWorldMessageType>(static_cast<eWorldMessageType>(y)), #z); ASSERT_EQ(magic_enum::enum_name<eWorldMessageType>(static_cast<eWorldMessageType>(y)), #z);
#define log_test_invalid(y) LOG("%s", magic_enum::enum_name<eWorldMessageType>(static_cast<eWorldMessageType>(y))); ASSERT_EQ(magic_enum::enum_name<eWorldMessageType>(static_cast<eWorldMessageType>(y)), "<none>");

TEST(MagicEnumTest, eWorldMessageTypeTest) {
	//LOG("%s %s", magic_enum::enum_name<eWorldMessageType>(static_cast<eWorldMessageType>(1)), eWorldMessageType::VALIDATION); ASSERT_EQ(magic_enum::enum_name<eWorldMessageType>(static_cast<eWorldMessageType>(1)), eWorldMessageType::VALIDATION)
	log_test(1, eWorldMessageType::VALIDATION);
	log_test(2, eWorldMessageType::CHARACTER_LIST_REQUEST);
	log_test(3, eWorldMessageType::CHARACTER_CREATE_REQUEST);
	log_test(4, eWorldMessageType::LOGIN_REQUEST);
	log_test(5, eWorldMessageType::GAME_MSG);
	log_test(6, eWorldMessageType::CHARACTER_DELETE_REQUEST);
	log_test(7, eWorldMessageType::CHARACTER_RENAME_REQUEST);
	log_test(8, eWorldMessageType::HAPPY_FLOWER_MODE_NOTIFY);
	log_test(9, eWorldMessageType::SLASH_RELOAD_MAP);
	log_test(10, eWorldMessageType::SLASH_PUSH_MAP_REQUEST);
	log_test(11, eWorldMessageType::SLASH_PUSH_MAP);
	log_test(12, eWorldMessageType::SLASH_PULL_MAP);
	log_test(13, eWorldMessageType::LOCK_MAP_REQUEST);
	log_test(14, eWorldMessageType::GENERAL_CHAT_MESSAGE);
	log_test(15, eWorldMessageType::HTTP_MONITOR_INFO_REQUEST);
	log_test(16, eWorldMessageType::SLASH_DEBUG_SCRIPTS);
	log_test(17, eWorldMessageType::MODELS_CLEAR);
	log_test(18, eWorldMessageType::EXHIBIT_INSERT_MODEL);
	log_test(19, eWorldMessageType::LEVEL_LOAD_COMPLETE);
	log_test(20, eWorldMessageType::TMP_GUILD_CREATE);
	log_test(21, eWorldMessageType::ROUTE_PACKET);
	log_test(22, eWorldMessageType::POSITION_UPDATE);
	log_test(23, eWorldMessageType::MAIL);
	log_test(24, eWorldMessageType::WORD_CHECK);
	log_test(25, eWorldMessageType::STRING_CHECK);
	log_test(26, eWorldMessageType::GET_PLAYERS_IN_ZONE);
	log_test(27, eWorldMessageType::REQUEST_UGC_MANIFEST_INFO);
	log_test(28, eWorldMessageType::BLUEPRINT_GET_ALL_DATA_REQUEST);
	log_test(29, eWorldMessageType::CANCEL_MAP_QUEUE);
	log_test(30, eWorldMessageType::HANDLE_FUNNESS);
	log_test(31, eWorldMessageType::FAKE_PRG_CSR_MESSAGE);
	log_test(32, eWorldMessageType::REQUEST_FREE_TRIAL_REFRESH);
	log_test(33, eWorldMessageType::GM_SET_FREE_TRIAL_STATUS);
	log_test(91, eWorldMessageType::UI_HELP_TOP_5);
	log_test_invalid(123);

	auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 10000000; ++i) {
		auto f = magic_enum::enum_name<eWorldMessageType>(static_cast<eWorldMessageType>(i));

		// To ensure the compiler doesn't optimize out the call, I print it at random intervals
		if (rand() % 100000 == 0) LOG("%i, %s", i, f);
	}
	auto end = std::chrono::high_resolution_clock::now();
	LOG("Time: %lld", std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());

	delete Game::logger;
}
