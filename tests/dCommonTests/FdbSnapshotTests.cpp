#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "FdbSnapshot.h"
#include "../FdbTestWriter.h"

namespace {
	using FdbTestWriter::Value;

	std::vector<FdbTestWriter::Table> Tables(int32_t extraRows, const std::string& name = "first") {
		FdbTestWriter::Table objects{ "Objects", { { "id", eSqliteDataType::INT32 }, { "name", eSqliteDataType::TEXT_4 } }, 4, {} };
		objects.rows.push_back({ Value::Int(1), Value::Text(name) });
		for (int32_t i = 0; i < extraRows; i++) objects.rows.push_back({ Value::Int(100 + i), Value::Text("x") });
		FdbTestWriter::Table other{ "Other", { { "id", eSqliteDataType::INT32 } }, 2, { { Value::Int(5) } } };
		return { objects, other };
	}

	class FdbSnapshotTest : public ::testing::Test {
	protected:
		void SetUp() override {
			m_Dir = std::filesystem::temp_directory_path() / ("dlu_fdb_snapshot_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
			std::filesystem::remove_all(m_Dir);
			std::filesystem::create_directories(m_Dir / "res");
			std::filesystem::create_directories(m_Dir / "resServer");
		}
		void TearDown() override { std::filesystem::remove_all(m_Dir); }

		std::filesystem::path Client() const { return m_Dir / "res" / "cdclient.fdb"; }
		std::filesystem::path Server() const { return m_Dir / "resServer"; }

		std::filesystem::path m_Dir;
	};
}

TEST(FdbSnapshotNames, HashIsFnv1a64) {
	EXPECT_EQ(FdbSnapshot::Hash(nullptr, 0), 14695981039346656037ULL);
	const std::string a = "a";
	EXPECT_EQ(FdbSnapshot::Hash(reinterpret_cast<const uint8_t*>(a.data()), a.size()), 0xaf63dc4c8601ec8cULL);
}

TEST(FdbSnapshotNames, NamesRoundTrip) {
	const uint64_t hash = 0x0123456789abcdefULL;
	EXPECT_EQ(FdbSnapshot::FdbName(hash), "cdclient-0123456789abcdef.fdb");
	EXPECT_EQ(FdbSnapshot::SqliteName(hash), "CDServer-0123456789abcdef.sqlite");
	EXPECT_EQ(FdbSnapshot::ParseName(FdbSnapshot::FdbName(hash)), hash);
	EXPECT_EQ(FdbSnapshot::ParseName(FdbSnapshot::SqliteName(hash)), hash);
	EXPECT_FALSE(FdbSnapshot::ParseName("cdclient.fdb"));
	EXPECT_FALSE(FdbSnapshot::ParseName("CDServer.sqlite"));
	EXPECT_FALSE(FdbSnapshot::ParseName("cdclient-0123456789ABCDEF.fdb"));
	EXPECT_FALSE(FdbSnapshot::ParseName("cdclient-0123.fdb"));
}

TEST_F(FdbSnapshotTest, CopyIsNamedByItsContent) {
	FdbTestWriter::WriteFile(Client(), FdbTestWriter::Write(Tables(0)));
	std::string error;
	const auto hash = FdbSnapshot::MakeCopy(Client(), Server(), error);
	ASSERT_TRUE(hash) << error;
	EXPECT_EQ(hash, FdbSnapshot::HashFile(Client()));
	EXPECT_TRUE(std::filesystem::exists(Server() / FdbSnapshot::FdbName(*hash)));

	// The same bytes again keep the one copy
	EXPECT_EQ(FdbSnapshot::MakeCopy(Client(), Server(), error), hash);

	// New bytes, new name; the old copy stays until it is removed
	FdbTestWriter::WriteFile(Client(), FdbTestWriter::Write(Tables(1)));
	const auto next = FdbSnapshot::MakeCopy(Client(), Server(), error);
	ASSERT_TRUE(next);
	EXPECT_NE(*next, *hash);
	EXPECT_TRUE(std::filesystem::exists(Server() / FdbSnapshot::FdbName(*hash)));
	EXPECT_TRUE(std::filesystem::exists(Server() / FdbSnapshot::FdbName(*next)));

	uint32_t files = 0;
	for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator(Server())) files++;
	EXPECT_EQ(files, 2u) << "no temporary copies left behind";
}

TEST_F(FdbSnapshotTest, MissingSourceFails) {
	std::string error;
	EXPECT_FALSE(FdbSnapshot::MakeCopy(Client(), Server(), error));
	EXPECT_FALSE(error.empty());
}

TEST_F(FdbSnapshotTest, CurrentRoundTrip) {
	EXPECT_FALSE(FdbSnapshot::ReadCurrent(Server()));
	const FdbSnapshot::Current current{ FdbSnapshot::FdbName(1), FdbSnapshot::SqliteName(1) };
	ASSERT_TRUE(FdbSnapshot::WriteCurrent(Server(), current));
	EXPECT_EQ(FdbSnapshot::ReadCurrent(Server()), current);
	const FdbSnapshot::Current next{ FdbSnapshot::FdbName(2), FdbSnapshot::DEFAULT_SQLITE };
	ASSERT_TRUE(FdbSnapshot::WriteCurrent(Server(), next));
	EXPECT_EQ(FdbSnapshot::ReadCurrent(Server()), next);

	// A pointer file naming something outside resServer is ignored
	ASSERT_TRUE(FdbSnapshot::WriteCurrent(Server(), { "../cdclient.fdb", "CDServer.sqlite" }));
	EXPECT_FALSE(FdbSnapshot::ReadCurrent(Server()));
}

TEST_F(FdbSnapshotTest, RemoveOldKeepsCurrentAndUnrelatedFiles) {
	for (const uint64_t hash : { 1, 2, 3 }) {
		FdbTestWriter::WriteFile(Server() / FdbSnapshot::FdbName(hash), { 1 });
		FdbTestWriter::WriteFile(Server() / FdbSnapshot::SqliteName(hash), { 1 });
	}
	FdbTestWriter::WriteFile(Server() / "CDServer.sqlite", { 1 });
	FdbTestWriter::WriteFile(Server() / "cdclient-.abc.tmp", { 1 });
	FdbTestWriter::WriteFile(Server() / "notes.txt", { 1 });

	const auto removed = FdbSnapshot::RemoveOld(Server(), { 2, 3 });
	EXPECT_EQ(removed.size(), 3u);
	EXPECT_FALSE(std::filesystem::exists(Server() / FdbSnapshot::FdbName(1)));
	EXPECT_FALSE(std::filesystem::exists(Server() / FdbSnapshot::SqliteName(1)));
	EXPECT_TRUE(std::filesystem::exists(Server() / FdbSnapshot::FdbName(2)));
	EXPECT_TRUE(std::filesystem::exists(Server() / FdbSnapshot::SqliteName(3)));
	EXPECT_TRUE(std::filesystem::exists(Server() / "CDServer.sqlite"));
	EXPECT_TRUE(std::filesystem::exists(Server() / "notes.txt"));
}

TEST(FdbSnapshotWatcher, TriggersOnceASettledChangeIsSeen) {
	FdbSnapshot::Watcher watcher;
	const FdbSnapshot::Stamp first{ 100, 1, true };
	watcher.Accept(first);
	EXPECT_FALSE(watcher.Poll(first));

	// Being written: changes between polls, so wait
	const FdbSnapshot::Stamp writing{ 50, 2, true };
	const FdbSnapshot::Stamp written{ 120, 3, true };
	EXPECT_FALSE(watcher.Poll(writing));
	EXPECT_FALSE(watcher.Poll(written));
	EXPECT_TRUE(watcher.Poll(written));
	// Until it is accepted, it keeps asking (a failed reload is retried)
	EXPECT_TRUE(watcher.Poll(written));
	watcher.Accept(written);
	EXPECT_FALSE(watcher.Poll(written));

	// A missing file (mid-replace) never triggers
	const FdbSnapshot::Stamp missing{};
	EXPECT_FALSE(watcher.Poll(missing));
	EXPECT_FALSE(watcher.Poll(missing));
	// And a file put back as it was isn't a change
	EXPECT_FALSE(watcher.Poll(written));
	EXPECT_FALSE(watcher.Poll(written));
}

TEST_F(FdbSnapshotTest, StampFollowsTheFile) {
	EXPECT_FALSE(FdbSnapshot::StampOf(Client()).exists);
	FdbTestWriter::WriteFile(Client(), FdbTestWriter::Write(Tables(0)));
	const auto stamp = FdbSnapshot::StampOf(Client());
	EXPECT_TRUE(stamp.exists);
	EXPECT_EQ(stamp.size, std::filesystem::file_size(Client()));
}

TEST_F(FdbSnapshotTest, DescribeChangesNamesChangedTables) {
	FdbTestWriter::WriteFile(Client(), FdbTestWriter::Write(Tables(0)));
	const auto before = FdbSnapshot::Summarize(Client());
	ASSERT_EQ(before.size(), 2u);
	EXPECT_EQ(before.at("Objects").rows, 1u);

	EXPECT_TRUE(FdbSnapshot::DescribeChanges(before, before).empty());

	FdbTestWriter::WriteFile(Client(), FdbTestWriter::Write(Tables(2)));
	auto lines = FdbSnapshot::DescribeChanges(before, FdbSnapshot::Summarize(Client()));
	ASSERT_EQ(lines.size(), 1u);
	EXPECT_EQ(lines[0], "Objects: 1 -> 3 rows");

	FdbTestWriter::WriteFile(Client(), FdbTestWriter::Write(Tables(0, "renamed")));
	lines = FdbSnapshot::DescribeChanges(before, FdbSnapshot::Summarize(Client()));
	ASSERT_EQ(lines.size(), 1u);
	EXPECT_EQ(lines[0], "Objects: 1 -> 1 rows (values changed)");
}

TEST_F(FdbSnapshotTest, ResolveNeverNamesTheClientsFile) {
	auto resolved = FdbSnapshot::Resolve(Server());
	EXPECT_EQ(resolved.sqlite, Server() / "CDServer.sqlite");
	EXPECT_TRUE(resolved.fdb.empty());

	// Named but not there yet: still the defaults
	ASSERT_TRUE(FdbSnapshot::WriteCurrent(Server(), { FdbSnapshot::FdbName(7), FdbSnapshot::SqliteName(7) }));
	EXPECT_TRUE(FdbSnapshot::Resolve(Server()).fdb.empty());

	FdbTestWriter::WriteFile(Server() / FdbSnapshot::FdbName(7), { 1 });
	FdbTestWriter::WriteFile(Server() / FdbSnapshot::SqliteName(7), { 1 });
	resolved = FdbSnapshot::Resolve(Server());
	EXPECT_EQ(resolved.sqlite, Server() / FdbSnapshot::SqliteName(7));
	EXPECT_EQ(resolved.fdb, Server() / FdbSnapshot::FdbName(7));
}
