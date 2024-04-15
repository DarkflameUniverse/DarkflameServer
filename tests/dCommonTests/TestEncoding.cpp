#include <string>
#include <gtest/gtest.h>
#include <string_view>

#include "GeneralUtils.h"

class EncodingTest : public ::testing::Test {
protected:
	std::string originalWord;
	std::string_view originalWordSv;
	uint32_t out;
};

TEST_F(EncodingTest, TestEncodingHello) {
	originalWord = "Hello World!";
	originalWordSv = originalWord;

	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'H');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'e');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'l');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'l');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'o');
	EXPECT_EQ(GeneralUtils::details::_NextUTF8Char(originalWordSv, out), true);

	EXPECT_EQ(GeneralUtils::UTF8ToUTF16("Hello World!"), u"Hello World!");
};

TEST_F(EncodingTest, TestEncodingUmlaut) {
	originalWord = reinterpret_cast<const char*>(u8"Frühling");
	originalWordSv = originalWord;

	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'F');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'r');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'ü');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'h');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'l');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'i');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'n');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'g');
	EXPECT_EQ(GeneralUtils::details::_NextUTF8Char(originalWordSv, out), false);

	EXPECT_EQ(GeneralUtils::UTF8ToUTF16("Frühling"), u"Frühling");
};

TEST_F(EncodingTest, TestEncodingChinese) {
	originalWord = "中文字";
	originalWordSv = originalWord;

	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'中');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'文');
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'字');
	EXPECT_EQ(GeneralUtils::details::_NextUTF8Char(originalWordSv, out), false);

	EXPECT_EQ(GeneralUtils::UTF8ToUTF16("中文字"), u"中文字");
};

TEST_F(EncodingTest, TestEncodingEmoji) {
	originalWord = "👨‍⚖️";
	originalWordSv = originalWord;

	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 0x1F468);
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 0x200D);
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 0x2696);
	GeneralUtils::details::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 0xFE0F);
	EXPECT_EQ(GeneralUtils::details::_NextUTF8Char(originalWordSv, out), false);

	EXPECT_EQ(GeneralUtils::UTF8ToUTF16("👨‍⚖️"), u"👨‍⚖️");
};
