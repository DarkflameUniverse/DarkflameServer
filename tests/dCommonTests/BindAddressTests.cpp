#include <gtest/gtest.h>

#include "GeneralUtils.h"

TEST(BindAddressTests, EmptyMeansAllInterfaces) {
	EXPECT_EQ(GeneralUtils::ParseBindAddress(""), std::string{});
	EXPECT_EQ(GeneralUtils::ParseBindAddress("   "), std::string{});
	EXPECT_EQ(GeneralUtils::ParseBindAddress("0.0.0.0"), std::string{});
	EXPECT_EQ(GeneralUtils::ParseBindAddress("*"), std::string{});
}

TEST(BindAddressTests, AcceptsIPv4) {
	EXPECT_EQ(GeneralUtils::ParseBindAddress("127.0.0.1"), "127.0.0.1");
	EXPECT_EQ(GeneralUtils::ParseBindAddress(" 192.168.1.20\r"), "192.168.1.20");
	EXPECT_EQ(GeneralUtils::ParseBindAddress("255.255.255.255"), "255.255.255.255");
	EXPECT_EQ(GeneralUtils::ParseBindAddress("localhost"), "127.0.0.1");
	EXPECT_EQ(GeneralUtils::ParseBindAddress("LocalHost"), "127.0.0.1");
}

TEST(BindAddressTests, RejectsEverythingElse) {
	for (const auto* bad : { "256.0.0.1", "1.2.3", "1.2.3.4.5", "1..2.3", "a.b.c.d", "010.0.0.1", "-1.0.0.1",
		"1.2.3.4:2000", "example.com", "::1", "1.2.3.4 5", "+1.2.3.4" }) {
		EXPECT_FALSE(GeneralUtils::ParseBindAddress(bad).has_value()) << bad;
	}
}
