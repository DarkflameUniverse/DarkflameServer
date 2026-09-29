#include <gtest/gtest.h>

#include "ClientSysInfo.h"

using ClientSysInfo::ParseMemoryStats;

namespace {
	// As the client writes it: each number right-aligned to 7 characters, no separators, " \n" after the vmem total
	constexpr std::string_view FULL = "123456789 p,234567890 vbytes.45 n-use.16717048 TKb-pmem.8123456 FKb pmem.33434096 TKb pfile."
		"20000000 FKb pfile.4194176 TKbytes vmem. \n3800000 FKb vmem.P 130000000 p,240000000 v.";
}

TEST(ClientSysInfoTests, ReadsEveryPart) {
	const auto s = ParseMemoryStats(FULL);
	EXPECT_TRUE(s.complete);
	EXPECT_EQ(s.workingSetBytes, 123456789);
	EXPECT_EQ(s.pagefileUsageBytes, 234567890);
	EXPECT_EQ(s.memoryLoadPercent, 45);
	EXPECT_EQ(s.totalPhysKb, 16717048);
	EXPECT_EQ(s.availPhysKb, 8123456);
	EXPECT_EQ(s.totalPageFileKb, 33434096);
	EXPECT_EQ(s.availPageFileKb, 20000000);
	EXPECT_EQ(s.totalVirtualKb, 4194176);
	EXPECT_EQ(s.availVirtualKb, 3800000);
	EXPECT_EQ(s.peakWorkingSetBytes, 130000000);
	EXPECT_EQ(s.peakPagefileUsageBytes, 240000000);
}

TEST(ClientSysInfoTests, PaddedNumbers) {
	const auto s = ParseMemoryStats("  12345 p,  67890 vbytes.7 n-use.1048576 TKb-pmem.  52428 FKb pmem.");
	EXPECT_FALSE(s.complete);
	EXPECT_EQ(s.workingSetBytes, 12345);
	EXPECT_EQ(s.pagefileUsageBytes, 67890);
	EXPECT_EQ(s.memoryLoadPercent, 7);
	EXPECT_EQ(s.totalPhysKb, 1048576);
	EXPECT_EQ(s.availPhysKb, 52428);
	EXPECT_FALSE(s.totalPageFileKb);
}

TEST(ClientSysInfoTests, CutTextKeepsWhatWasRead) {
	// The client stops at 255 characters: a part cut in half is left out, with everything after it
	const std::string cut(FULL.substr(0, FULL.find("FKb vmem.") + 3));
	const auto s = ParseMemoryStats(cut);
	EXPECT_FALSE(s.complete);
	EXPECT_EQ(s.totalVirtualKb, 4194176);
	EXPECT_FALSE(s.availVirtualKb);
	EXPECT_FALSE(s.peakWorkingSetBytes);
}

TEST(ClientSysInfoTests, OtherTextReadsNothing) {
	for (const auto* text : { "", "garbage", "12 q,34 vbytes.", "p, vbytes." }) {
		const auto s = ParseMemoryStats(text);
		EXPECT_FALSE(s.complete) << text;
		EXPECT_FALSE(s.workingSetBytes) << text;
		EXPECT_FALSE(s.totalPhysKb) << text;
	}
}
