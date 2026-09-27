#include <gtest/gtest.h>

#include "dCommonVars.h"

TEST(LUString33Test, SerializeWriteTestOld) {
    RakNet::BitStream bitStream;
    std::string testString;
    for (int i = 0; i < 33; i++) testString += "a";
    for (const auto& c : testString) bitStream.Write(c);
    std::string result;
    char c;
    while (bitStream.Read(c)) result += c;
    ASSERT_EQ(result, testString);
}

TEST(LUString33Test, SerializeWriteTestOldPartial) {
    RakNet::BitStream bitStream;
    std::string testString;
    for (int i = 0; i < 15; i++) testString += "a";
    for (const auto& c : testString) bitStream.Write(c);
    for (int i = 0; i < 18; i++) bitStream.Write<char>(0);
    std::string result;
    char c;
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

TEST(LUString33Test, SerializeWriteTestNew) {
    RakNet::BitStream bitStream;
    std::string testString;
    for (int i = 0; i < 33; i++) testString += "a";
    bitStream.Write(LUString(testString, 33));
    std::string result;
    char c;
    while (bitStream.Read(c)) result += c;
    ASSERT_EQ(result, testString);
}

TEST(LUString33Test, SerializeWriteTestNewPartial) {
    RakNet::BitStream bitStream;
    std::string testString;
    for (int i = 0; i < 15; i++) testString += "a";
    bitStream.Write(LUString(testString, 33));
    std::string result;
    char c;
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

TEST(LUString33Test, SerializeReadTestOld) {
    RakNet::BitStream bitStream;
    std::string testString;
    for (int i = 0; i < 33; i++) testString += "a";
    for (const auto& c : testString) bitStream.Write(c);
    std::string result;
    char c;
    while (bitStream.Read(c)) result += c;
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
    ASSERT_EQ(result, testString);
}

TEST(LUString33Test, SerializeReadTestOldPartial) {
    RakNet::BitStream bitStream;
    std::string testString;
    for (int i = 0; i < 15; i++) testString += "a";
    for (const auto& c : testString) bitStream.Write(c);
    for (int i = 0; i < 18; i++) bitStream.Write<char>(0);
    std::string result;
    char c;
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

TEST(LUString33Test, SerializeReadTestNew) {
    RakNet::BitStream bitStream;
    std::string testString;
    for (int i = 0; i < 33; i++) testString += "a";
    bitStream.Write(LUString(testString, 33));
	LUString result;
	ASSERT_EQ(result.size, 33);
	ASSERT_TRUE(bitStream.Read(result));
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
    ASSERT_EQ(result.string, testString);
}

TEST(LUString33Test, SerializeReadTestNewPartial) {
    RakNet::BitStream bitStream;
    std::string testString;
    for (int i = 0; i < 15; i++) testString += "a";
    bitStream.Write(LUString(testString, 33));
	LUString result;
	ASSERT_EQ(result.size, 33);
	ASSERT_TRUE(bitStream.Read(result));
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
    ASSERT_EQ(result.string, testString);
}
