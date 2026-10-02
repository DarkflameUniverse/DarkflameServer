#include <gtest/gtest.h>

#include "DatabaseUsageReport.h"

using namespace DatabaseUsageReport;

TEST(DatabaseUsageReport, LocalHosts) {
	EXPECT_TRUE(IsLocalHost(""));
	EXPECT_TRUE(IsLocalHost("localhost"));
	EXPECT_TRUE(IsLocalHost("LOCALHOST:3306"));
	EXPECT_TRUE(IsLocalHost("127.0.0.1"));
	EXPECT_TRUE(IsLocalHost("tcp://127.0.0.1:3307"));
	EXPECT_TRUE(IsLocalHost("[::1]:3306"));
	EXPECT_TRUE(IsLocalHost("::1"));
	EXPECT_TRUE(IsLocalHost("unix:///run/mysqld/mysqld.sock"));
	EXPECT_TRUE(IsLocalHost("pipe://MySQL"));
	EXPECT_FALSE(IsLocalHost("db.example.com"));
	EXPECT_FALSE(IsLocalHost("tcp://10.0.0.5:3306"));
	EXPECT_FALSE(IsLocalHost("192.168.1.2"));
}

TEST(DatabaseUsageReport, DiskWarning) {
	EXPECT_FALSE(DiskState(0, 0, 10).known);
	const auto low = DiskState(5, 100, 10);
	EXPECT_TRUE(low.known);
	EXPECT_TRUE(low.low);
	EXPECT_DOUBLE_EQ(low.freePercent, 5.0);
	EXPECT_FALSE(DiskState(10, 100, 10).low);
	EXPECT_FALSE(DiskState(5, 100, 0).low); // 0: never warn
	EXPECT_EQ(DiskState(200, 100, 10).free, 100u);
	const auto json = TotalsJson({ 7, 1000, 0, 600, 400, 3, 5, 100 }, 10);
	EXPECT_TRUE(json["disk"]["low"].get<bool>());
	EXPECT_EQ(json["total_bytes"].get<uint64_t>(), 1000u);
}

TEST(DatabaseUsageReport, SeriesKeepsTheNewestSampleOfEachSlice) {
	std::vector<IDatabaseUsage::UsageTotals> samples;
	for (int64_t i = 0; i < 100; i++) samples.push_back({ i * 3600, static_cast<uint64_t>(i), 0, 0, 0, 0, 0, i % 2 ? 0u : 50u });
	const auto all = Series(samples, 200);
	EXPECT_EQ(all["times"].size(), 100u);
	EXPECT_TRUE(all["disk_free"][1].is_null()); // disk not known then
	const auto few = Series(samples, 10);
	EXPECT_LE(few["times"].size(), 10u);
	EXPECT_GE(few["times"].size(), 9u);
	EXPECT_EQ(few["times"].back().get<int64_t>(), 99 * 3600);
	EXPECT_EQ(few["total"].back().get<uint64_t>(), 99u);
	for (size_t i = 1; i < few["times"].size(); i++) EXPECT_LT(few["times"][i - 1].get<int64_t>(), few["times"][i].get<int64_t>());
	EXPECT_TRUE(Series({}, 10)["times"].empty());
}

TEST(DatabaseUsageReport, GrowthPerTable) {
	const std::vector<IDatabaseUsage::TableUsage> before = { { "chat_log", 1000, 100, 10 }, { "accounts", 500, 500, 2 }, { "gone", 9, 0, 1 } };
	const std::vector<IDatabaseUsage::TableUsage> now = { { "chat_log", 5000, 300, 50 }, { "accounts", 400, 500, 2 }, { "new_table", 200, 0, 4 } };
	const auto tables = Tables(now, before, 2 * 86400);
	ASSERT_EQ(tables.size(), 3u);
	EXPECT_EQ(tables[0]["name"], "chat_log"); // biggest first
	EXPECT_EQ(tables[0]["growth_bytes"].get<int64_t>(), 4200);
	EXPECT_EQ(tables[0]["growth_rows"].get<int64_t>(), 40);
	EXPECT_DOUBLE_EQ(tables[0]["growth_per_day"].get<double>(), 2100.0);
	EXPECT_EQ(tables[1]["growth_bytes"].get<int64_t>(), -100); // accounts shrank
	EXPECT_EQ(tables[2]["growth_bytes"].get<int64_t>(), 200); // new: all of it

	const auto fastest = Fastest(tables, 1);
	ASSERT_EQ(fastest.size(), 1u);
	EXPECT_EQ(fastest[0]["name"], "chat_log");
	EXPECT_EQ(Fastest(tables, 10).size(), 2u); // the shrinking table is left out
}
