#include <gtest/gtest.h>

#include "dCommonVars.h"

TEST(LUWString33Test, SerializeWriteTestOld) {
    RakNet::BitStream bitStream;
    std::u16string testString;
    for (int i = 0; i < 33; i++) testString += u'ü';
    for (const auto& c : testString) bitStream.Write(c);
    std::u16string result;
    char16_t c;
    while (bitStream.Read(c)) result += c;
    ASSERT_EQ(result, testString);
}

TEST(LUWString33Test, SerializeWriteTestOldPartial) {
    RakNet::BitStream bitStream;
    std::u16string testString;
    for (int i = 0; i < 15; i++) testString += u'ü';
    for (const auto& c : testString) bitStream.Write(c);
    for (int i = 0; i < 18; i++) bitStream.Write<char16_t>(0);
    std::u16string result;
    char16_t c;
	int nulls = 18;
    while (bitStream.Read(c)){
        if (c == 0) {
			nulls--;
			continue;
		}
        result += c;
    }
	ASSERT_EQ(nulls, 0);
    ASSERT_EQ(result, testString);
}

TEST(LUWString33Test, SerializeWriteTestNew) {
    RakNet::BitStream bitStream;
    std::u16string testString;
    for (int i = 0; i < 33; i++) testString += u'ü';
    bitStream.Write(LUWString(testString, 33));
    std::u16string result;
    char16_t c;
    while (bitStream.Read(c)) result += c;
    ASSERT_EQ(result, testString);
}

TEST(LUWString33Test, SerializeWriteTestNewPartial) {
    RakNet::BitStream bitStream;
    std::u16string testString;
    for (int i = 0; i < 15; i++) testString += u'ü';
    bitStream.Write(LUWString(testString, 33));
    std::u16string result;
    char16_t c;
	int nulls = 18;
    while (bitStream.Read(c)){
        if (c == 0) {
			nulls--;
			continue;
		}
        result += c;
    }
	ASSERT_EQ(nulls, 0);
    ASSERT_EQ(result, testString);
}

TEST(LUWString33Test, SerializeReadTestOld) {
    RakNet::BitStream bitStream;
    std::u16string testString;
    for (int i = 0; i < 33; i++) testString += u'ü';
    for (const auto& c : testString) bitStream.Write(c);
    std::u16string result;
    char16_t c;
    while (bitStream.Read(c)) result += c;
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
    ASSERT_EQ(result, testString);
}

TEST(LUWString33Test, SerializeReadTestOldPartial) {
    RakNet::BitStream bitStream;
    std::u16string testString;
    for (int i = 0; i < 15; i++) testString += u'ü';
    for (const auto& c : testString) bitStream.Write(c);
    for (int i = 0; i < 18; i++) bitStream.Write<char16_t>(0);
    std::u16string result;
    char16_t c;
	int nulls = 18;
    while (bitStream.Read(c)){
        if (c == 0) {
			nulls--;
			continue;
		}
        result += c;
    }
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
	ASSERT_EQ(nulls, 0);
    ASSERT_EQ(result, testString);
}

TEST(LUWString33Test, SerializeReadTestNew) {
    RakNet::BitStream bitStream;
    std::u16string testString;
    for (int i = 0; i < 33; i++) testString += u'ü';
    bitStream.Write(LUWString(testString, 33));
	LUWString result;
	ASSERT_EQ(result.size, 33);
	ASSERT_TRUE(bitStream.Read(result));
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
    ASSERT_EQ(result.string, testString);
}

TEST(LUWString33Test, SerializeReadTestNewPartial) {
    RakNet::BitStream bitStream;
    std::u16string testString;
    for (int i = 0; i < 15; i++) testString += u'ü';
    bitStream.Write(LUWString(testString, 33));
	LUWString result;
	ASSERT_EQ(result.size, 33);
	ASSERT_TRUE(bitStream.Read(result));
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
    ASSERT_EQ(result.string, testString);
}
