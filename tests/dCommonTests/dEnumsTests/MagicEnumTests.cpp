#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include "StringifiedEnum.h"
#include "Logger.h"
#include "Game.h"
#include "MessageType/Game.h"
#include "MessageType/World.h"
#include "magic_enum.hpp"

#define ENUM_EQ(e, y, z)\
	LOG("%s %s", StringifiedEnum::ToString(static_cast<e>(y)).data(), #z);\
	ASSERT_STREQ(StringifiedEnum::ToString(static_cast<e>(y)).data(), #z);

#define ENUM_NE(e, y)\
	ENUM_EQ(e, y, UNKNOWN);

// Test World Message Enum Reflection
TEST(MagicEnumTest, eWorldMessageTypeTest) {
	Game::logger = new Logger("./MagicEnumTest_eWorldMessageTypeTest.log", true, true);

	ENUM_EQ(MessageType::World, 1, VALIDATION);
	ENUM_EQ(MessageType::World, 2, CHARACTER_LIST_REQUEST);
	ENUM_EQ(MessageType::World, 3, CHARACTER_CREATE_REQUEST);
	ENUM_EQ(MessageType::World, 4, LOGIN_REQUEST);
	ENUM_EQ(MessageType::World, 5, GAME_MSG);
	ENUM_EQ(MessageType::World, 6, CHARACTER_DELETE_REQUEST);
	ENUM_EQ(MessageType::World, 7, CHARACTER_RENAME_REQUEST);
	ENUM_EQ(MessageType::World, 8, HAPPY_FLOWER_MODE_NOTIFY);
	ENUM_EQ(MessageType::World, 9, SLASH_RELOAD_MAP);
	ENUM_EQ(MessageType::World, 10, SLASH_PUSH_MAP_REQUEST);
	ENUM_EQ(MessageType::World, 11, SLASH_PUSH_MAP);
	ENUM_EQ(MessageType::World, 12, SLASH_PULL_MAP);
	ENUM_EQ(MessageType::World, 13, LOCK_MAP_REQUEST);
	ENUM_EQ(MessageType::World, 14, GENERAL_CHAT_MESSAGE);
	ENUM_EQ(MessageType::World, 15, HTTP_MONITOR_INFO_REQUEST);
	ENUM_EQ(MessageType::World, 16, SLASH_DEBUG_SCRIPTS);
	ENUM_EQ(MessageType::World, 17, MODELS_CLEAR);
	ENUM_EQ(MessageType::World, 18, EXHIBIT_INSERT_MODEL);
	ENUM_EQ(MessageType::World, 19, LEVEL_LOAD_COMPLETE);
	ENUM_EQ(MessageType::World, 20, TMP_GUILD_CREATE);
	ENUM_EQ(MessageType::World, 21, ROUTE_PACKET);
	ENUM_EQ(MessageType::World, 22, POSITION_UPDATE);
	ENUM_EQ(MessageType::World, 23, MAIL);
	ENUM_EQ(MessageType::World, 24, WORD_CHECK);
	ENUM_EQ(MessageType::World, 25, STRING_CHECK);
	ENUM_EQ(MessageType::World, 26, GET_PLAYERS_IN_ZONE);
	ENUM_EQ(MessageType::World, 27, REQUEST_UGC_MANIFEST_INFO);
	ENUM_EQ(MessageType::World, 28, BLUEPRINT_GET_ALL_DATA_REQUEST);
	ENUM_EQ(MessageType::World, 29, CANCEL_MAP_QUEUE);
	ENUM_EQ(MessageType::World, 30, HANDLE_FUNNESS);
	ENUM_EQ(MessageType::World, 31, FAKE_PRG_CSR_MESSAGE);
	ENUM_EQ(MessageType::World, 32, REQUEST_FREE_TRIAL_REFRESH);
	ENUM_EQ(MessageType::World, 33, GM_SET_FREE_TRIAL_STATUS);
	ENUM_EQ(MessageType::World, 91, UI_HELP_TOP_5);
	ENUM_NE(MessageType::World, 37);
	ENUM_NE(MessageType::World, 123);

	srand(time(NULL));
	auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 10000000; ++i) {
		volatile auto f = StringifiedEnum::ToString(static_cast<MessageType::World>(i)).data();

		// To ensure the compiler doesn't optimize out the call, I print it at random intervals
		if (rand() % 100000 == 0) LOG("%i, %s", i, f);
	}
	auto end = std::chrono::high_resolution_clock::now();
	LOG("Time: %lld", std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());

	delete Game::logger;
}

// Test Game Message Enum Reflection
TEST(MagicEnumTest, eGameMessageTypeTest) {

	Game::logger = new Logger("./MagicEnumTest_eGameMessageTypeTest.log", true, true);

	// Only doing the first and last 10 for the sake of my sanity
	ENUM_EQ(MessageType::Game, 0, GET_POSITION);
	ENUM_EQ(MessageType::Game, 1, GET_ROTATION);
	ENUM_EQ(MessageType::Game, 2, GET_LINEAR_VELOCITY);
	ENUM_EQ(MessageType::Game, 3, GET_ANGULAR_VELOCITY);
	ENUM_EQ(MessageType::Game, 4, GET_FORWARD_VELOCITY);
	ENUM_EQ(MessageType::Game, 5, GET_PLAYER_FORWARD);
	ENUM_EQ(MessageType::Game, 6, GET_FORWARD_VECTOR);
	ENUM_EQ(MessageType::Game, 7, SET_POSITION);
	ENUM_EQ(MessageType::Game, 8, SET_LOCAL_POSITION);
	ENUM_EQ(MessageType::Game, 9, SET_ROTATION);
	ENUM_EQ(MessageType::Game, 10, SET_LINEAR_VELOCITY);
	ENUM_EQ(MessageType::Game, 1762, USE_SKILL_SET);
	ENUM_EQ(MessageType::Game, 1763, SET_SKILL_SET_POSSESSOR);
	ENUM_EQ(MessageType::Game, 1764, POPULATE_ACTION_BAR);
	ENUM_EQ(MessageType::Game, 1765, GET_COMPONENT_TEMPLATE_ID);
	ENUM_EQ(MessageType::Game, 1766, GET_POSSESSABLE_SKILL_SET);
	ENUM_EQ(MessageType::Game, 1767, MARK_INVENTORY_ITEM_AS_ACTIVE);
	ENUM_EQ(MessageType::Game, 1768, UPDATE_FORGED_ITEM);
	ENUM_EQ(MessageType::Game, 1769, CAN_ITEMS_BE_REFORGED);
	ENUM_EQ(MessageType::Game, 1771, NOTIFY_CLIENT_RAIL_START_FAILED);
	ENUM_EQ(MessageType::Game, 1772, GET_IS_ON_RAIL);
	ENUM_NE(MessageType::Game, 32);
	ENUM_NE(MessageType::Game, 1776);

	srand(time(NULL));
	auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 10000000; ++i) {
		volatile auto f = StringifiedEnum::ToString(static_cast<MessageType::Game>(i)).data();

		// To ensure the compiler doesn't optimize out the call, I print it at random intervals
		if (rand() % 100000 == 0) LOG("%i, %s", i, f);
	}
	auto end = std::chrono::high_resolution_clock::now();
	LOG("Time: %lld", std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());

	delete Game::logger;
}

#define LOG_EARRAY(EARRAY_VAR, INDICE, ENTRY) LOG(#EARRAY_VAR"[%i] = %i, %s", INDICE, ENTRY, magic_enum::enum_name(ENTRY).data());

namespace {
	template <typename T>
	void AssertEnumArraySorted(const T& eArray) {
		for (int i = 0; i < eArray->size() - 1; ++i) {
			const auto entryCurr = eArray->at(i).first;
			LOG_EARRAY(eArray, i, entryCurr);
			const auto entryNext = eArray->at(++i).first;
			LOG_EARRAY(eArray, i, entryNext);
			ASSERT_TRUE(entryCurr < entryNext);
		}
	}
}

// Test that the magic enum arrays are pre-sorted
TEST(MagicEnumTest, ArraysAreSorted) {
	Game::logger = new Logger("./MagicEnumTest_ArraysAreSorted.log", true, true);

	constexpr auto wmArray = &magic_enum::enum_entries<MessageType::World>();
	AssertEnumArraySorted(wmArray);

	constexpr auto gmArray = &magic_enum::enum_entries<MessageType::Game>();
	AssertEnumArraySorted(gmArray);

	delete Game::logger;
}
