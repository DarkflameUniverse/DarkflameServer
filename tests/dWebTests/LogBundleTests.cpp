#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>

#include "LogBundle.h"
#include "ZCompression.h"

namespace fs = std::filesystem;
using namespace LogBundle;

namespace {
	// "[dd-mm-yy HH:MM:SS " for a Unix time, in local time as the servers write it
	std::string Stamp(int64_t time) {
		const auto t = static_cast<std::time_t>(time);
		std::tm tm{};
#ifdef _WIN32
		localtime_s(&tm, &t);
#else
		localtime_r(&t, &tm);
#endif
		char text[32];
		std::strftime(text, sizeof(text), "[%d-%m-%y %H:%M:%S ", &tm);
		return text;
	}

	void SetWritten(const fs::path& path, int64_t time) {
		const auto sys = std::chrono::system_clock::time_point(std::chrono::seconds(time));
		fs::last_write_time(path, std::chrono::clock_cast<fs::file_time_type::clock>(sys));
	}

	// The entries of a zip file, inflated
	std::map<std::string, std::string> ReadZip(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		const std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		const auto u16 = [&](size_t at) { return static_cast<uint32_t>(static_cast<uint8_t>(data[at]) | static_cast<uint8_t>(data[at + 1]) << 8); };
		const auto u32 = [&](size_t at) { return u16(at) | u16(at + 2) << 16; };
		std::map<std::string, std::string> entries;
		const auto end = data.size() - 22;
		EXPECT_EQ(u32(end), 0x06054b50u);
		size_t at = u32(end + 16);
		for (uint32_t i = 0; i < u16(end + 10); i++) {
			EXPECT_EQ(u32(at), 0x02014b50u);
			const auto crc = u32(at + 16), compressed = u32(at + 20), size = u32(at + 24), nameLength = u16(at + 28), offset = u32(at + 42);
			const auto name = data.substr(at + 46, nameLength);
			const auto body = offset + 30 + u16(offset + 26) + u16(offset + 28);
			auto content = ZCompression::InflateRaw(std::string_view(data).substr(body, compressed), size);
			EXPECT_TRUE(content.has_value()) << name;
			if (content) {
				EXPECT_EQ(ZCompression::Crc32(0, *content), crc) << name;
				entries[name] = *content;
			}
			at += 46 + nameLength + u16(at + 30) + u16(at + 32);
		}
		return entries;
	}

	class LogBundleTest : public ::testing::Test {
	protected:
		void SetUp() override {
			m_Root = fs::temp_directory_path() / ("log_bundle_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
				::testing::UnitTest::GetInstance()->current_test_info()->name());
			fs::remove_all(m_Root);
			fs::create_directories(m_Root / "logs");
			fs::create_directories(m_Root / "dumps");
		}
		void TearDown() override { fs::remove_all(m_Root); }

		fs::path Make(const fs::path& relative, const std::string& content, int64_t written) {
			const auto path = m_Root / relative;
			fs::create_directories(path.parent_path());
			std::ofstream(path, std::ios::binary) << content;
			SetWritten(path, written);
			return path;
		}

		std::vector<std::string> Names(const Filter& filter) {
			std::vector<std::string> names;
			for (const auto& file : Select(m_Root / "logs", m_Root / "dumps", filter)) names.push_back(file.archiveName);
			return names;
		}

		fs::path m_Root;
	};

	constexpr int64_t T = 1790000000; // a start time
}

TEST(LogBundleNames, ParsesServerAndWorldNames) {
	auto name = ParseLogName("MasterServer_1790520594.log");
	ASSERT_TRUE(name);
	EXPECT_EQ(name->server, "MasterServer");
	EXPECT_EQ(name->started, 1790520594);
	EXPECT_FALSE(name->zone);

	name = ParseLogName("WorldServer_1000_2_3_1790520596.log");
	ASSERT_TRUE(name);
	EXPECT_EQ(name->server, "WorldServer");
	EXPECT_EQ(name->zone, 1000u);
	EXPECT_EQ(name->clone, 2u);
	EXPECT_EQ(name->instance, 3u);
	EXPECT_EQ(name->started, 1790520596);

	EXPECT_FALSE(ParseLogName("MasterServer_1790520594.txt"));
	EXPECT_FALSE(ParseLogName("SomethingElse_1790520594.log"));
	EXPECT_FALSE(ParseLogName("WorldServer_1000_2_1790520596.log"));
	EXPECT_FALSE(ParseLogName("AuthServer_abc.log"));
}

TEST(LogBundleNames, ParsesCrashDumps) {
	auto name = ParseCrashName("Crash_WorldServer_1200_0_4_1790520596_31337.log");
	ASSERT_TRUE(name);
	EXPECT_EQ(name->server, "WorldServer");
	EXPECT_EQ(name->zone, 1200u);
	name = ParseCrashName("Crash_UgcServer_1790520596_42.log");
	ASSERT_TRUE(name);
	EXPECT_EQ(name->server, "UgcServer");
	EXPECT_EQ(name->started, 1790520596);
	name = ParseCrashName("Crash_WorldServer_42.log"); // crashed before it knew its zone
	ASSERT_TRUE(name);
	EXPECT_FALSE(name->zone);
	EXPECT_FALSE(ParseCrashName("WorldServer_42.log"));
}

TEST(LogBundleFilter, ReadsTheQuery) {
	std::map<std::string, std::string> query{ {"from", "100"}, {"to", "200"}, {"servers", "world,auth"}, {"zones", "1000,1200"},
		{"instance", "3"}, {"crash", "1"}, {"trim", "1"}, {"text", "Error"}, {"redact", "1"} };
	const auto value = [&](const std::string& key) { return query.contains(key) ? query[key] : std::string(); };
	std::string error;
	const auto filter = Filter::FromQuery(value, error);
	ASSERT_TRUE(filter) << error;
	EXPECT_EQ(filter->from, 100);
	EXPECT_EQ(filter->to, 200);
	EXPECT_EQ(filter->servers, (std::set<std::string>{ "WorldServer", "AuthServer" }));
	EXPECT_EQ(filter->zones, (std::set<uint32_t>{ 1000, 1200 }));
	EXPECT_FALSE(filter->clone);
	EXPECT_EQ(filter->instance, 3u);
	EXPECT_TRUE(filter->crashDumps && filter->trim && filter->redactIps && filter->FiltersLines());
	EXPECT_EQ(filter->text, "error");

	for (const auto& [key, bad] : std::map<std::string, std::string>{ {"from", "yesterday"}, {"servers", "world,moon"}, {"zones", "1000,x"}, {"clone", "-1"} }) {
		query = { {key, bad} };
		error.clear();
		EXPECT_FALSE(Filter::FromQuery(value, error)) << key;
		EXPECT_FALSE(error.empty()) << key;
	}
	query = { {"from", "300"}, {"to", "200"} };
	EXPECT_FALSE(Filter::FromQuery(value, error));
}

TEST_F(LogBundleTest, SelectsByDateServerAndWorld) {
	Make("logs/MasterServer/MasterServer_" + std::to_string(T) + ".log", "m\n", T + 3600);
	Make("logs/AuthServer/AuthServer_" + std::to_string(T + 7200) + ".log", "a\n", T + 9000);
	Make("logs/WorldServer/1000/0/WorldServer_1000_0_1_" + std::to_string(T) + ".log", "w\n", T + 600);
	Make("logs/WorldServer/1200/1/WorldServer_1200_1_2_" + std::to_string(T) + ".log", "w\n", T + 600);
	Make("logs/ChatServer_" + std::to_string(T - 86400) + ".log", "old layout\n", T - 80000);
	Make("logs/notes.txt", "not a log\n", T);
	Make("dumps/Crash_WorldServer_1200_1_2_" + std::to_string(T) + "_99.log", "backtrace\n", T + 700);

	Filter all;
	EXPECT_EQ(Names(all).size(), 5u); // no crash dumps unless asked, and not notes.txt
	all.crashDumps = true;
	EXPECT_EQ(Names(all).size(), 6u);

	// Overlap: the master log (T to T+3600) matches a range inside it; the auth log starts after it
	Filter range;
	range.from = T + 1000;
	range.to = T + 2000;
	EXPECT_EQ(Names(range), std::vector<std::string>{ "logs/MasterServer/MasterServer_" + std::to_string(T) + ".log" });

	Filter worlds;
	worlds.servers = { "WorldServer" };
	worlds.zones = { 1200 };
	worlds.crashDumps = true;
	EXPECT_EQ(Names(worlds), (std::vector<std::string>{ "crash_dumps/Crash_WorldServer_1200_1_2_" + std::to_string(T) + "_99.log",
		"logs/WorldServer/1200/1/WorldServer_1200_1_2_" + std::to_string(T) + ".log" }));
	worlds.clone = 0;
	EXPECT_TRUE(Names(worlds).empty());

	// Old files at the top of logs/ count too
	Filter chat;
	chat.servers = { "ChatServer" };
	EXPECT_EQ(Names(chat), std::vector<std::string>{ "logs/ChatServer_" + std::to_string(T - 86400) + ".log" });
}

TEST(LogBundleLines, ReadsLineTimes) {
	EXPECT_EQ(LineTime(Stamp(T) + "dConfig.cpp:97] hi"), T);
	EXPECT_FALSE(LineTime("  continued line"));
	EXPECT_FALSE(LineTime("[ab-cd-ef 00:00:00 x"));
}

TEST(LogBundleLines, RedactsAddresses) {
	EXPECT_EQ(RedactIps("login from 192.168.1.20 ok"), "login from [ip] ok");
	EXPECT_EQ(RedactIps("peer 10.0.0.5:2001 connected."), "peer [ip]:2001 connected.");
	EXPECT_EQ(RedactIps("from 2001:db8::1 and ::1."), "from [ip] and [ip].");
	EXPECT_EQ(RedactIps("full fe80:0:0:0:202:b3ff:fe1e:8329 x"), "full [ip] x");
	// Not addresses
	EXPECT_EQ(RedactIps("[27-09-26 09:49:54 dConfig.cpp:97] v1.2.3 999.1.1.1"), "[27-09-26 09:49:54 dConfig.cpp:97] v1.2.3 999.1.1.1");
	EXPECT_EQ(RedactIps("Foo::Bar called at 12:30:45"), "Foo::Bar called at 12:30:45");
}

TEST_F(LogBundleTest, ZipsWholeAndFilteredFiles) {
	const std::string log = Stamp(T) + "a.cpp:1] start from 1.2.3.4\n" + Stamp(T + 100) + "a.cpp:2] ERROR one\n  detail of one\n" +
		Stamp(T + 200) + "a.cpp:3] fine\n" + Stamp(T + 300) + "a.cpp:4] error two\n";
	Make("logs/AuthServer/AuthServer_" + std::to_string(T) + ".log", log, T + 300);
	Make("logs/ChatServer/ChatServer_" + std::to_string(T) + ".log", Stamp(T) + "c.cpp:1] nothing\n", T + 300);
	const auto out = m_Root / "bundle.zip";

	Filter whole;
	auto files = Select(m_Root / "logs", m_Root / "dumps", whole);
	auto result = WriteZip(out, files, whole, "header\n", 1024 * 1024);
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.files, 2u);
	auto entries = ReadZip(out);
	EXPECT_EQ(entries["logs/AuthServer/AuthServer_" + std::to_string(T) + ".log"], log);
	EXPECT_TRUE(entries["manifest.txt"].starts_with("header\n"));
	EXPECT_NE(entries["manifest.txt"].find("logs/ChatServer/ChatServer_"), std::string::npos);
	EXPECT_EQ(result.archiveSize, fs::file_size(out));

	// Lines from T+50 to T+250 with "error", addresses hidden: the continued line keeps the time of the one before
	Filter lines;
	lines.from = T + 50;
	lines.to = T + 250;
	lines.trim = true;
	lines.text = "error";
	lines.redactIps = true;
	result = WriteZip(out, files, lines, "header\n", 1024 * 1024);
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_EQ(result.files, 1u); // nothing of the chat log is left, so it stays out
	entries = ReadZip(out);
	EXPECT_EQ(entries["logs/AuthServer/AuthServer_" + std::to_string(T) + ".log"], Stamp(T + 100) + "a.cpp:2] ERROR one\n");
	EXPECT_FALSE(entries.contains("logs/ChatServer/ChatServer_" + std::to_string(T) + ".log"));

	lines = {};
	lines.redactIps = true;
	result = WriteZip(out, files, lines, "", 1024 * 1024);
	ASSERT_TRUE(result.ok);
	EXPECT_NE(ReadZip(out)["logs/AuthServer/AuthServer_" + std::to_string(T) + ".log"].find("start from [ip]"), std::string::npos);
}

TEST_F(LogBundleTest, StopsAtTheSizeLimit) {
	Make("logs/MasterServer/MasterServer_" + std::to_string(T) + ".log", std::string(200 * 1024, 'x'), T);
	const auto out = m_Root / "bundle.zip";
	Filter filter;
	const auto result = WriteZip(out, Select(m_Root / "logs", {}, filter), filter, "", 100 * 1024);
	EXPECT_FALSE(result.ok);
	EXPECT_TRUE(result.overLimit);
	EXPECT_NE(result.error.find("log_bundle_max_mb"), std::string::npos);
	EXPECT_FALSE(fs::exists(out));
}

TEST(LogBundleCompression, DeflatesInPieces) {
	std::string compressed;
	ZCompression::RawDeflater deflater([&](std::string_view data) { compressed += data; return true; });
	std::string input;
	for (int i = 0; i < 5000; i++) input += "line " + std::to_string(i) + " of the log\n";
	for (size_t at = 0; at < input.size(); at += 1000) ASSERT_TRUE(deflater.Write(std::string_view(input).substr(at, 1000)));
	ASSERT_TRUE(deflater.Finish());
	EXPECT_EQ(deflater.BytesIn(), input.size());
	EXPECT_EQ(deflater.BytesOut(), compressed.size());
	EXPECT_LT(compressed.size(), input.size() / 3);
	EXPECT_EQ(ZCompression::InflateRaw(compressed, input.size()), input);
}
