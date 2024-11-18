#include <gtest/gtest.h>

#include "GeneralUtils.h"

#include "eGameMasterLevel.h"
#include "MessageType/Game.h"
#include "MessageType/World.h"

#define ASSERT_TYPE_EQ(TYPE, ENUM)\
	ASSERT_TRUE(typeid(TYPE) == typeid(GeneralUtils::ToUnderlying(static_cast<ENUM>(0))));

#define ASSERT_TYPE_NE(TYPE, ENUM)\
	ASSERT_FALSE(typeid(TYPE) == typeid(GeneralUtils::ToUnderlying(static_cast<ENUM>(0))));

// Verify that the underlying enum types are being cast correctly
TEST(ToUnderlyingTests, VerifyToUnderlying) {
	ASSERT_TYPE_EQ(uint8_t, eGameMasterLevel);
	ASSERT_TYPE_EQ(uint16_t, MessageType::Game);
	ASSERT_TYPE_EQ(uint32_t, MessageType::World)

	ASSERT_TYPE_NE(void, eGameMasterLevel);
	ASSERT_TYPE_NE(void, MessageType::Game);
	ASSERT_TYPE_NE(void, MessageType::World)
}
