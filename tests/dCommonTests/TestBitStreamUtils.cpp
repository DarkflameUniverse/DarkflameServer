#include <gtest/gtest.h>

#include "BitStreamUtils.h"

#include <vector>

namespace {
	std::vector<uint8_t> Bytes(RakNet::BitStream& bitStream) {
		return { bitStream.GetData(), bitStream.GetData() + bitStream.GetNumberOfBytesUsed() };
	}
}

// The helper must produce exactly what the hand written "size then one char at a time" loops produce.
TEST(BitStreamUtilsTests, WriteLengthPrefixedU16MatchesPerCharacterLoop) {
	const std::u16string value = u"sfx/countdown";

	RakNet::BitStream expected;
	expected.Write(true); // misalign the stream by one bit, like a leading bool field would
	expected.Write<uint32_t>(value.size());
	for (const auto character : value) expected.Write(character);

	RakNet::BitStream actual;
	actual.Write(true);
	BitStreamUtils::WriteLengthPrefixed(actual, value);

	ASSERT_EQ(actual.GetNumberOfBitsUsed(), expected.GetNumberOfBitsUsed());
	ASSERT_EQ(Bytes(actual), Bytes(expected));
}

TEST(BitStreamUtilsTests, WriteLengthPrefixedStringGolden) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteLengthPrefixed<uint16_t>(bitStream, std::string("ab"));
	const std::vector<uint8_t> golden = { 0x02, 0x00, 'a', 'b' };
	ASSERT_EQ(Bytes(bitStream), golden);
}

TEST(BitStreamUtilsTests, LengthPrefixedRoundTrip) {
	const std::u16string value = u"Hello ü";
	RakNet::BitStream bitStream;
	bitStream.Write(false);
	BitStreamUtils::WriteLengthPrefixed(bitStream, value);
	BitStreamUtils::WriteLengthPrefixed(bitStream, std::u16string());

	bool leading = true;
	std::u16string read = u"garbage";
	std::u16string empty = u"garbage";
	ASSERT_TRUE(bitStream.Read(leading));
	ASSERT_TRUE(BitStreamUtils::ReadLengthPrefixed(bitStream, read));
	ASSERT_TRUE(BitStreamUtils::ReadLengthPrefixed(bitStream, empty));
	EXPECT_FALSE(leading);
	EXPECT_EQ(read, value);
	EXPECT_TRUE(empty.empty());
	EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST(BitStreamUtilsTests, ReadLengthPrefixedRejectsBadLengths) {
	{
		RakNet::BitStream bitStream;
		bitStream.Write<int32_t>(-1);
		std::u16string out;
		EXPECT_FALSE(BitStreamUtils::ReadLengthPrefixed<int32_t>(bitStream, out));
	}
	{
		RakNet::BitStream bitStream;
		bitStream.Write<uint32_t>(11);
		std::string out;
		EXPECT_FALSE(BitStreamUtils::ReadLengthPrefixed(bitStream, out, 10));
	}
	{
		// Claims 4 characters but only has 1.
		RakNet::BitStream bitStream;
		bitStream.Write<uint32_t>(4);
		bitStream.Write<char16_t>(u'a');
		std::u16string out;
		EXPECT_FALSE(BitStreamUtils::ReadLengthPrefixed(bitStream, out));
	}
}

// WriteOptional must match the hand written "flag, then value if not default" pattern.
TEST(BitStreamUtilsTests, WriteOptionalMatchesHandWrittenPattern) {
	for (const int32_t value : { 0, -1, 7 }) {
		RakNet::BitStream expected;
		expected.Write(value != -1);
		if (value != -1) expected.Write(value);

		RakNet::BitStream actual;
		BitStreamUtils::WriteOptional<int32_t>(actual, value, -1);
		ASSERT_EQ(actual.GetNumberOfBitsUsed(), expected.GetNumberOfBitsUsed());
		ASSERT_EQ(Bytes(actual), Bytes(expected));

		int32_t read = 12345;
		ASSERT_TRUE(BitStreamUtils::ReadOptional<int32_t>(actual, read, -1));
		EXPECT_EQ(read, value);
		EXPECT_EQ(actual.GetNumberOfUnreadBits(), 0);
	}
}

TEST(BitStreamUtilsTests, WriteOptionalGolden) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteOptional(bitStream, 3.0f, 3.0f); // default: a single 0 bit
	BitStreamUtils::WriteOptional(bitStream, 1.0f, 3.0f); // 1 bit, then 00 00 80 3f
	// 0 1 00000000 00000000 10000000 00111111 (MSB first) padded
	const std::vector<uint8_t> golden = { 0x40, 0x00, 0x20, 0x0f, 0xc0 };
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 34);
	ASSERT_EQ(Bytes(bitStream), golden);
}

TEST(BitStreamUtilsTests, ReadOptionalFailsOnTruncatedValue) {
	RakNet::BitStream bitStream;
	bitStream.Write(true);
	bitStream.Write<uint8_t>(1);
	uint32_t out{};
	EXPECT_FALSE(BitStreamUtils::ReadOptional<uint32_t>(bitStream, out, 0));
}
