#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "FdbReader.h"
#include "../FdbTestWriter.h"

using FdbTestWriter::Value;

namespace {
	std::vector<FdbTestWriter::Table> SampleTables() {
		FdbTestWriter::Table items;
		items.name = "Items";
		items.columns = {
			{ "id", eSqliteDataType::INT32 },
			{ "name", eSqliteDataType::TEXT_4 },
			{ "big", eSqliteDataType::INT64 },
			{ "scale", eSqliteDataType::REAL },
			{ "flag", eSqliteDataType::INT_BOOL },
			{ "notes", eSqliteDataType::TEXT_8 },
		};
		items.bucketCount = 4;
		// 1, 5 and -3 all land in bucket 1
		items.rows = {
			{ Value::Int(1), Value::Text("one"), Value::Int64(0x123456789ALL), Value::Real(1.5f), Value::Bool(true), Value::Text("a", true) },
			{ Value::Int(2), Value::Text("two"), Value::Int64(-2), Value::Real(-0.25f), Value::Bool(false), Value::Null() },
			{ Value::Int(5), Value::Text("caf\xE9"), Value::Null(), Value::Null(), Value::Null(), Value::Text("") },
			{ Value::Int(1), Value::Text("one again"), Value::Int64(1), Value::Real(2.0f), Value::Bool(false), Value::Null() },
			{ Value::Int(-3), Value::Text("negative"), Value::Int64(INT64_MIN), Value::Real(3.0f), Value::Bool(true), Value::Text("42abc") },
		};

		FdbTestWriter::Table empty;
		empty.name = "Empty";
		empty.columns = { { "id", eSqliteDataType::INT32 } };
		empty.bucketCount = 0;

		return { empty, items };
	}

	class FdbReaderTest : public ::testing::TestWithParam<bool> {
	protected:
		void SetUp() override {
			// Parameterized test names contain '/'
			std::string name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
			for (auto& c : name) if (c == '/') c = '_';
			m_Path = std::filesystem::temp_directory_path() / ("dlu_fdb_reader_" + name + ".fdb");
			FdbTestWriter::WriteFile(m_Path, FdbTestWriter::Write(SampleTables()));
		}

		void TearDown() override {
			m_Reader.Close();
			std::error_code error;
			std::filesystem::remove(m_Path, error);
		}

		std::vector<std::string> NamesWithKey(const FdbReader::Table& table, int64_t key) {
			std::vector<std::string> names;
			table.ForEachRowWithKey(key, [&](const FdbReader::Row& row) { names.push_back(row.GetString(1)); });
			return names;
		}

		std::filesystem::path m_Path;
		FdbReader m_Reader;
	};
}

// true maps the file, false reads it into memory (the fallback)
INSTANTIATE_TEST_SUITE_P(MapOrRead, FdbReaderTest, ::testing::Values(true, false));

TEST_P(FdbReaderTest, OpensAndFindsTables) {
	ASSERT_TRUE(m_Reader.Open(m_Path, GetParam()));
	EXPECT_EQ(m_Reader.IsMapped(), GetParam());
	EXPECT_EQ(m_Reader.GetTables().size(), 2u);

	const auto* items = m_Reader.GetTable("Items");
	ASSERT_NE(items, nullptr);
	EXPECT_EQ(items->GetBucketCount(), 4u);
	ASSERT_EQ(items->GetColumns().size(), 6u);
	EXPECT_EQ(items->GetColumns()[2].name, "big");
	EXPECT_EQ(items->GetColumns()[2].type, eSqliteDataType::INT64);
	EXPECT_EQ(items->GetColumnIndex("notes"), 5);
	EXPECT_EQ(items->GetColumnIndex("missing"), -1);

	EXPECT_EQ(m_Reader.GetTable("items"), nullptr);
	EXPECT_EQ(m_Reader.GetTable("Nope"), nullptr);

	const auto* empty = m_Reader.GetTable("Empty");
	ASSERT_NE(empty, nullptr);
	EXPECT_FALSE(empty->FindFirst(0).has_value());
	uint32_t rows = 0;
	empty->ForEachRow([&](const FdbReader::Row&) { rows++; });
	EXPECT_EQ(rows, 0u);
}

TEST_P(FdbReaderTest, LooksUpByKeyThroughCollisions) {
	ASSERT_TRUE(m_Reader.Open(m_Path, GetParam()));
	const auto& items = *m_Reader.GetTable("Items");

	// Same bucket, filtered by key, file order kept
	EXPECT_EQ(NamesWithKey(items, 1), (std::vector<std::string>{ "one", "one again" }));
	EXPECT_EQ(NamesWithKey(items, 5), (std::vector<std::string>{ "caf\xC3\xA9" }));
	EXPECT_EQ(NamesWithKey(items, -3), (std::vector<std::string>{ "negative" }));
	EXPECT_EQ(NamesWithKey(items, 2), (std::vector<std::string>{ "two" }));
	EXPECT_TRUE(NamesWithKey(items, 9).empty());
	EXPECT_TRUE(NamesWithKey(items, 3).empty());

	const auto first = items.FindFirst(1);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->GetString(1), "one");

	uint32_t rows = 0;
	items.ForEachRow([&](const FdbReader::Row&) { rows++; });
	EXPECT_EQ(rows, 5u);
}

TEST_P(FdbReaderTest, ReadsTypedValues) {
	ASSERT_TRUE(m_Reader.Open(m_Path, GetParam()));
	const auto& items = *m_Reader.GetTable("Items");

	const auto one = items.FindFirst(1);
	ASSERT_TRUE(one.has_value());
	EXPECT_EQ(one->GetFieldCount(), 6u);
	EXPECT_EQ(one->GetInt(0), 1);
	EXPECT_EQ(one->GetInt64(2), 0x123456789ALL);
	// Low 32 bits, as sqlite3_column_int
	EXPECT_EQ(one->GetInt(2), 0x3456789A);
	EXPECT_FLOAT_EQ(one->GetFloat(3), 1.5f);
	EXPECT_TRUE(one->GetBool(4));
	EXPECT_EQ(one->GetType(5), eSqliteDataType::TEXT_8);
	EXPECT_EQ(one->GetString(5), "a");

	const auto two = items.FindFirst(2);
	ASSERT_TRUE(two.has_value());
	EXPECT_EQ(two->GetInt64(2), -2);
	EXPECT_FLOAT_EQ(two->GetFloat(3), -0.25f);
	EXPECT_FALSE(two->GetBool(4, true));
	EXPECT_TRUE(two->IsNull(5));
	EXPECT_EQ(two->GetString(5, "fallback"), "fallback");

	const auto negative = items.FindFirst(-3);
	ASSERT_TRUE(negative.has_value());
	EXPECT_EQ(negative->GetInt64(2), INT64_MIN);
	// Text read as a number, as SQLite does
	EXPECT_EQ(negative->GetInt(5), 42);
	// A number read as text
	EXPECT_EQ(negative->GetString(0), "-3");

	// Nulls give the caller's default
	const auto five = items.FindFirst(5);
	ASSERT_TRUE(five.has_value());
	EXPECT_TRUE(five->IsNull(2));
	EXPECT_EQ(five->GetInt(2, -1), -1);
	EXPECT_EQ(five->GetInt64(2, -7), -7);
	EXPECT_FLOAT_EQ(five->GetFloat(3, -1.0f), -1.0f);
	EXPECT_TRUE(five->GetBool(4, true));
	EXPECT_EQ(five->GetRawString(1), "caf\xE9");
	EXPECT_EQ(five->GetString(5, "x"), "");

	// Past the last column reads as null
	EXPECT_TRUE(five->IsNull(99));
	EXPECT_EQ(five->GetInt(99, 11), 11);
}

TEST(FdbReaderFailureTest, MissingEmptyAndTruncatedFiles) {
	const auto dir = std::filesystem::temp_directory_path();
	FdbReader reader;
	EXPECT_FALSE(reader.Open(dir / "dlu_fdb_reader_does_not_exist.fdb"));
	EXPECT_FALSE(reader.IsOpen());

	const auto emptyPath = dir / "dlu_fdb_reader_empty.fdb";
	FdbTestWriter::WriteFile(emptyPath, {});
	EXPECT_FALSE(reader.Open(emptyPath));
	EXPECT_FALSE(reader.Open(emptyPath, false));

	// Cut before the row data: the headers are still sound, the rows read as missing instead of past the end
	auto bytes = FdbTestWriter::Write(SampleTables());
	const auto truncatedPath = dir / "dlu_fdb_reader_truncated.fdb";
	FdbTestWriter::WriteFile(truncatedPath, std::vector<uint8_t>(bytes.begin(), bytes.begin() + 20));
	EXPECT_FALSE(reader.Open(truncatedPath));

	// A table count far past the end of the file
	bytes[0] = 0xFF;
	bytes[1] = 0xFF;
	const auto badCountPath = dir / "dlu_fdb_reader_bad_count.fdb";
	FdbTestWriter::WriteFile(badCountPath, bytes);
	EXPECT_FALSE(reader.Open(badCountPath));

	std::error_code error;
	std::filesystem::remove(emptyPath, error);
	std::filesystem::remove(truncatedPath, error);
	std::filesystem::remove(badCountPath, error);
}

TEST(FdbReaderFailureTest, MappedFileMovesAndCloses) {
	const auto path = std::filesystem::temp_directory_path() / "dlu_fdb_mapped_move.fdb";
	FdbTestWriter::WriteFile(path, { 1, 2, 3, 4 });
	for (const bool map : { true, false }) {
		FdbMappedFile file;
		ASSERT_TRUE(file.Open(path, map));
		EXPECT_EQ(file.IsMapped(), map);
		EXPECT_EQ(file.GetSize(), 4u);

		FdbMappedFile moved(std::move(file));
		EXPECT_FALSE(file.IsOpen());
		ASSERT_TRUE(moved.IsOpen());
		EXPECT_EQ(moved.GetData()[3], 4);

		moved.Close();
		EXPECT_FALSE(moved.IsOpen());
		EXPECT_EQ(moved.GetSize(), 0u);
	}
	std::error_code error;
	std::filesystem::remove(path, error);
}
