#include <gtest/gtest.h>

#include <random>
#include <set>
#include <string>
#include <vector>

#include "BitStreamUtils.h"
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

TEST(GeneralUtilsTest, UnpredictableHexStringIsLowercaseHexOfTheAskedLength) {
	EXPECT_TRUE(GeneralUtils::GenerateUnpredictableHexString(0).empty());
	EXPECT_EQ(GeneralUtils::GenerateUnpredictableHexString(1).size(), 2);
	EXPECT_EQ(GeneralUtils::GenerateUnpredictableHexString(5).size(), 10);

	for (uint32_t i = 0; i < 16; i++) {
		const auto key = GeneralUtils::GenerateUnpredictableHexString(16);
		ASSERT_EQ(key.size(), 32);
		EXPECT_EQ(key.find_first_not_of("0123456789abcdef"), std::string::npos);
	}
}

TEST(GeneralUtilsTest, UnpredictableHexStringDiffersEveryTime) {
	std::set<std::string> seen;
	for (uint32_t i = 0; i < 64; i++) {
		// The same seed every time would give the same string every time from the game engine
		Game::randomEngine = std::mt19937(1234);
		seen.insert(GeneralUtils::GenerateUnpredictableHexString(16));
	}

	EXPECT_EQ(seen.size(), 64);
}

TEST(GeneralUtilsTest, UnpredictableHexStringFillsEveryCharacter) {
	// Every one of the 32 characters has to vary, none may be left at a fixed value
	std::vector<std::set<char>> seen(32);
	for (uint32_t i = 0; i < 64; i++) {
		const auto key = GeneralUtils::GenerateUnpredictableHexString(16);
		for (uint32_t j = 0; j < key.size(); j++) seen[j].insert(key[j]);
	}

	for (const auto& characters : seen) EXPECT_GT(characters.size(), 4);
}

TEST(GeneralUtilsTest, EqualsInConstantTime) {
	const std::string key = "0123456789abcdef0123456789abcdef";

	EXPECT_TRUE(GeneralUtils::EqualsInConstantTime(key, key));
	EXPECT_TRUE(GeneralUtils::EqualsInConstantTime(key, std::string(key)));
	EXPECT_TRUE(GeneralUtils::EqualsInConstantTime("", ""));

	// A difference anywhere, first, middle or last character
	EXPECT_FALSE(GeneralUtils::EqualsInConstantTime(key, "1123456789abcdef0123456789abcdef"));
	EXPECT_FALSE(GeneralUtils::EqualsInConstantTime(key, "0123456789abcdef1123456789abcdef"));
	EXPECT_FALSE(GeneralUtils::EqualsInConstantTime(key, "0123456789abcdef0123456789abcdee"));
	EXPECT_FALSE(GeneralUtils::EqualsInConstantTime(key, "0123456789ABCDEF0123456789ABCDEF"));

	// A prefix, a longer string and an empty one never match
	EXPECT_FALSE(GeneralUtils::EqualsInConstantTime(key, key.substr(0, 31)));
	EXPECT_FALSE(GeneralUtils::EqualsInConstantTime(key, key + "0"));
	EXPECT_FALSE(GeneralUtils::EqualsInConstantTime(key, ""));
	EXPECT_FALSE(GeneralUtils::EqualsInConstantTime("", key));

	// A zero byte inside the string is compared like any other
	EXPECT_FALSE(GeneralUtils::EqualsInConstantTime(std::string("ab\0d", 4), std::string("ab\0e", 4)));
}

// The key goes to the client as 33 wide characters, comes back the same way,
// and travels between the servers as 33 narrow ones
TEST(GeneralUtilsTest, SessionKeySurvivesItsPacketFields) {
	const auto key = GeneralUtils::GenerateUnpredictableHexString(16);

	RakNet::BitStream bitStream;
	bitStream.Write(LUWString(key));
	bitStream.Write(LUString(key));
	ASSERT_EQ(bitStream.GetNumberOfBytesUsed(), 33 * 2 + 33);

	LUWString fromClient;
	LUString fromServer;
	ASSERT_TRUE(bitStream.Read(fromClient));
	ASSERT_TRUE(bitStream.Read(fromServer));

	EXPECT_EQ(fromClient.GetAsString(), key);
	EXPECT_EQ(fromServer.string, key);
	EXPECT_TRUE(GeneralUtils::EqualsInConstantTime(fromClient.GetAsString(), fromServer.string));
}
