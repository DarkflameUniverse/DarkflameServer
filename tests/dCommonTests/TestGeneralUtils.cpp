#include <gtest/gtest.h>

#include <random>
#include <set>

#include "Game.h"
#include "GeneralUtils.h"

TEST(GeneralUtilsTest, UnpredictableNumberLeavesTheGameEngineAlone) {
	Game::randomEngine = std::mt19937(1234);
	std::mt19937 expected(1234);

	GeneralUtils::GenerateUnpredictableNumber<uint32_t>();
	GeneralUtils::GenerateUnpredictableNumber<uint64_t>();

	EXPECT_EQ(Game::randomEngine(), expected());
}

TEST(GeneralUtilsTest, UnpredictableNumberDoesNotFollowTheGameEngineSeed) {
	std::set<uint64_t> seen;
	for (uint32_t i = 0; i < 16; i++) {
		// The same seed every time would give the same number every time from the game engine
		Game::randomEngine = std::mt19937(1234);
		seen.insert(GeneralUtils::GenerateUnpredictableNumber<uint64_t>());
	}

	EXPECT_EQ(seen.size(), 16);
}

TEST(GeneralUtilsTest, UnpredictableNumberFillsEveryByte) {
	uint64_t seenBits = 0;
	for (uint32_t i = 0; i < 64; i++) seenBits |= GeneralUtils::GenerateUnpredictableNumber<uint64_t>();

	EXPECT_EQ(seenBits, 0xFFFFFFFFFFFFFFFFULL);
}
