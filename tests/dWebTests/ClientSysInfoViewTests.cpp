#include <gtest/gtest.h>

#include "ClientSysInfoView.h"
#include "Permissions.h"

namespace {
	IClientSysInfo::SysInfoRow Row(uint32_t account, uint32_t build, const std::string& card, uint32_t cpus, uint64_t memoryKb) {
		IClientSysInfo::SysInfoRow r;
		r.id = account;
		r.accountId = account;
		r.firstSeen = 1700000000;
		r.lastSeen = 1700000100;
		r.logins = 2;
		r.ip = "203.0.113.7";
		r.clientOs = 1;
		r.memoryStats = "  12345 p,  67890 vbytes.40 n-use." + std::to_string(memoryKb) + " TKb-pmem.";
		r.memoryTotalKb = memoryKb;
		r.videoCard = card;
		r.numberOfProcessors = cpus;
		r.processorType = 586;
		r.processorLevel = 6;
		r.processorRevision = 0x9e0a;
		r.osVersionInfoSize = 276;
		r.majorVersion = build == 2600 ? 5 : 6;
		r.minorVersion = build == 2600 ? 1 : 2;
		r.buildNumber = build;
		r.platformId = 2;
		return r;
	}
}

// What GET /api/accounts/:id/client_sysinfo sends for each row
TEST(ClientSysInfoViewTests, RowKeepsTheRawValues) {
	const auto row = Row(7, 9200, "NVIDIA GeForce GTX 1080 (HAL-hw vp)", 16, 16717048);
	const auto json = ClientSysInfoView::RowJson(row, true);
	EXPECT_EQ(json["memory_stats"], row.memoryStats);
	EXPECT_EQ(json["video_card"], row.videoCard);
	EXPECT_EQ(json["processor_type"], 586);
	EXPECT_EQ(json["processor_revision"], 0x9e0a);
	EXPECT_EQ(json["processor_model"], 0x9e);
	EXPECT_EQ(json["processor_stepping"], 0x0a);
	EXPECT_EQ(json["os_version"], "6.2.9200");
	EXPECT_EQ(json["os_version_info_size"], 276);
	EXPECT_EQ(json["memory"]["total_phys_kb"], 16717048);
	EXPECT_EQ(json["memory"]["memory_load_percent"], 40);
	EXPECT_TRUE(json["memory"]["avail_phys_kb"].is_null()); // not in the text
	EXPECT_EQ(json["memory_bucket"], "8-16 GB");
	EXPECT_EQ(json["ip"], "203.0.113.7");
}

TEST(ClientSysInfoViewTests, AddressOnlyWithLogsAudit) {
	const auto json = ClientSysInfoView::RowJson(Row(7, 9200, "Card", 4, 0), false);
	EXPECT_FALSE(json.contains("ip"));
	EXPECT_EQ(json["memory_bucket"], "Not read");
}

TEST(ClientSysInfoViewTests, EveryShownFieldHasACaveat) {
	const auto& caveats = ClientSysInfoView::Caveats();
	for (const auto* key : { "ip", "clientOs", "memoryStats", "videoCard", "numberOfProcessors", "processorType", "processorLevel",
		"processorRevision", "osVersionInfoSize", "osVersion", "platformId" }) {
		ASSERT_TRUE(caveats.contains(key)) << key;
		EXPECT_FALSE(caveats[key].get<std::string>().empty()) << key;
	}
	// The Windows version caveat says why it's not the real version
	EXPECT_NE(caveats["osVersion"].get<std::string>().find("6.2 build 9200"), std::string::npos);
}

TEST(ClientSysInfoViewTests, OsLabelsSayWhatIsReported) {
	EXPECT_NE(ClientSysInfoView::OsLabel(Row(1, 9200, "", 1, 0)).find("8.1, 10 and 11"), std::string::npos);
	EXPECT_NE(ClientSysInfoView::OsLabel(Row(1, 2600, "", 1, 0)).find("compatibility mode"), std::string::npos);
}

TEST(ClientSysInfoViewTests, MemoryBuckets) {
	constexpr uint64_t GB = 1024 * 1024;
	EXPECT_EQ(ClientSysInfoView::MemoryBucket(1 * GB), "Under 2 GB");
	EXPECT_EQ(ClientSysInfoView::MemoryBucket(4 * GB - 100000), "2-4 GB"); // a 4 GB machine reports a little under 4 GB
	EXPECT_EQ(ClientSysInfoView::MemoryBucket(16 * GB), "8-16 GB");
	EXPECT_EQ(ClientSysInfoView::MemoryBucket(33 * GB), "32-64 GB");
	EXPECT_EQ(ClientSysInfoView::MemoryBucket(128 * GB), "Over 64 GB");
}

// What GET /api/client_sysinfo/spread sends
TEST(ClientSysInfoViewTests, SpreadCountsEachAccountOnce) {
	constexpr uint64_t GB = 1024 * 1024;
	const std::vector<IClientSysInfo::SysInfoRow> latest{
		Row(1, 9200, "NVIDIA GeForce GTX 1080 (HAL-hw vp)", 16, 16 * GB),
		Row(2, 9200, "NVIDIA GeForce GTX 1080 (HAL-mixed vp)", 8, 8 * GB),
		Row(3, 2600, "AMD Radeon RX 580 (HAL-hw vp)", 16, 32 * GB),
	};
	const auto spread = ClientSysInfoView::Spread(latest);
	EXPECT_EQ(spread["accounts"], 3);
	// The same adapter with a different vertex processing mode is one card
	ASSERT_EQ(spread["video"].size(), 2u);
	EXPECT_EQ(spread["video"][0]["label"], "NVIDIA GeForce GTX 1080");
	EXPECT_EQ(spread["video"][0]["count"], 2);
	EXPECT_EQ(spread["os"][0]["count"], 2);
	EXPECT_NE(spread["os"][0]["label"].get<std::string>().find("6.2.9200"), std::string::npos);
	// Memory in size order, processors by count
	ASSERT_EQ(spread["memory"].size(), 3u);
	EXPECT_EQ(spread["memory"][0]["label"], "4-8 GB");
	EXPECT_EQ(spread["memory"][2]["label"], "16-32 GB");
	ASSERT_EQ(spread["processors"].size(), 2u);
	EXPECT_EQ(spread["processors"][0]["label"], "8");
	EXPECT_EQ(spread["processors"][1]["count"], 2);
	EXPECT_EQ(spread["clientOs"][0]["label"], "Windows");
}

TEST(ClientSysInfoViewTests, SpreadFoldsTheTail) {
	std::vector<IClientSysInfo::SysInfoRow> latest;
	for (uint32_t i = 0; i < 5; i++) latest.push_back(Row(i, 9200, "Card " + std::to_string(i), 4, 0));
	const auto spread = ClientSysInfoView::Spread(latest, 2);
	ASSERT_EQ(spread["video"].size(), 3u);
	EXPECT_EQ(spread["video"][2]["label"], "Other");
	EXPECT_EQ(spread["video"][2]["count"], 3);
}

TEST(ClientSysInfoViewTests, StaffOnlyPermission) {
	const auto* permission = Permissions::Find("client_sysinfo");
	ASSERT_NE(permission, nullptr);
	EXPECT_EQ(permission->category, "Accounts");
	EXPECT_GE(permission->defaultLevel, 1);
	EXPECT_FALSE(permission->locked); // can be granted per account
	ASSERT_NE(Permissions::Find("logs_audit"), nullptr);
}
