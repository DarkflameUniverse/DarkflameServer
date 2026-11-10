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

TEST(GeneralUtilsTest, StreamHoldsCountsTheBytesLeft) {
	RakNet::BitStream stream;
	stream.Write<uint32_t>(4);
	for (uint16_t i = 0; i < 4; i++) stream.Write<uint16_t>(u'a' + i);

	uint32_t length = 0;
	stream.Read(length);
	EXPECT_TRUE(GeneralUtils::StreamHolds(stream, 0));
	EXPECT_TRUE(GeneralUtils::StreamHolds(stream, 8));
	EXPECT_FALSE(GeneralUtils::StreamHolds(stream, 9));
	EXPECT_TRUE(GeneralUtils::StreamHolds(stream, 4, sizeof(uint16_t)));
	EXPECT_FALSE(GeneralUtils::StreamHolds(stream, 5, sizeof(uint16_t)));
	EXPECT_FALSE(GeneralUtils::StreamHolds(stream, UINT32_MAX, sizeof(uint16_t)));
	EXPECT_FALSE(GeneralUtils::StreamHolds(stream, UINT64_MAX, sizeof(uint16_t)));
	// What a negative 32 bit length becomes
	EXPECT_FALSE(GeneralUtils::StreamHolds(stream, static_cast<int32_t>(-1), sizeof(uint16_t)));
}

TEST(GeneralUtilsTest, ReadWStringReadsWhatTheStreamHolds) {
	RakNet::BitStream stream;
	stream.Write<uint32_t>(4);
	for (uint16_t i = 0; i < 4; i++) stream.Write<uint16_t>(u'a' + i);
	EXPECT_EQ(GeneralUtils::ReadWString(stream), u"abcd");

	RakNet::BitStream tooLong;
	tooLong.Write<uint32_t>(0xFFFFFFFF);
	tooLong.Write<uint16_t>(u'a');
	EXPECT_EQ(GeneralUtils::ReadWString(tooLong), u"");

	RakNet::BitStream empty;
	EXPECT_EQ(GeneralUtils::ReadWString(empty), u"");
}
