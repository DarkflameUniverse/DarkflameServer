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

	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'H');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'e');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'l');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'l');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 'o');
	EXPECT_EQ(GeneralUtils::_NextUTF8Char(originalWordSv, out), true);

	EXPECT_EQ(GeneralUtils::UTF8ToUTF16("Hello World!"), u"Hello World!");
};

TEST_F(EncodingTest, TestEncodingUmlaut) {
	originalWord = u8"Frühling";
	originalWordSv = originalWord;

	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'F');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'r');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'ü');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'h');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'l');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'i');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'n');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'g');
	EXPECT_EQ(GeneralUtils::_NextUTF8Char(originalWordSv, out), false);

	EXPECT_EQ(GeneralUtils::UTF8ToUTF16("Frühling"), u"Frühling");
};

TEST_F(EncodingTest, TestEncodingChinese) {
	originalWord = "中文字";
	originalWordSv = originalWord;

	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'中');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'文');
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, U'字');
	EXPECT_EQ(GeneralUtils::_NextUTF8Char(originalWordSv, out), false);

	EXPECT_EQ(GeneralUtils::UTF8ToUTF16("中文字"), u"中文字");
};

TEST_F(EncodingTest, TestEncodingEmoji) {
	originalWord = "👨‍⚖️";
	originalWordSv = originalWord;

	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 0x1F468);
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 0x200D);
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 0x2696);
	GeneralUtils::_NextUTF8Char(originalWordSv, out); EXPECT_EQ(out, 0xFE0F);
	EXPECT_EQ(GeneralUtils::_NextUTF8Char(originalWordSv, out), false);

	EXPECT_EQ(GeneralUtils::UTF8ToUTF16("👨‍⚖️"), u"👨‍⚖️");
};
