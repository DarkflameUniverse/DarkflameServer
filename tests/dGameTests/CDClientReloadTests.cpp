#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDClientSnapshot.h"
#include "CDComponentsRegistryTable.h"
#include "CDFdb.h"
#include "CDItemComponentTable.h"
#include "CDObjectsTable.h"
#include "Game.h"
#include "Logger.h"
#include "eReplicaComponentType.h"
#include "../FdbTestWriter.h"

using FdbTestWriter::Value;

namespace {
	int32_t Component(uint32_t id, int32_t type) {
		return CDComponentsRegistryTable::Instance().GetByIDAndType(id, static_cast<eReplicaComponentType>(type), -99);
	}

	std::vector<uint8_t> Registry(int32_t lot1Component) {
		FdbTestWriter::Table registry;
		registry.name = "ComponentsRegistry";
		registry.columns = { { "id", eSqliteDataType::INT32 }, { "component_type", eSqliteDataType::INT32 }, { "component_id", eSqliteDataType::INT32 } };
		registry.bucketCount = 4;
		registry.rows = {
			{ Value::Int(1), Value::Int(1), Value::Int(lot1Component) },
			{ Value::Int(5), Value::Int(1), Value::Int(50) },
		};
		return FdbTestWriter::Write({ registry });
	}

	class CDClientReloadTest : public ::testing::Test {
	protected:
		void SetUp() override {
			m_Logger = std::make_unique<Logger>("./testing.log", false, false);
			Game::logger = m_Logger.get();
			m_Dir = std::filesystem::temp_directory_path() / ("dlu_cdreload_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
			std::filesystem::remove_all(m_Dir);
			std::filesystem::create_directories(m_Dir / "res");
			std::filesystem::create_directories(m_Dir / "resServer");
			std::filesystem::create_directories(m_Dir / "migrations");
			// A cdserver migration that changes one row, like the real ones do
			std::ofstream(m_Dir / "migrations" / "1_change_lot_5.sql") << "UPDATE ComponentsRegistry SET component_id = 55 WHERE id = 5; -- a; comment\n";
		}

		void TearDown() override {
			CDFdb::Close();
			CDComponentsRegistryTable::Instance().LoadFromFdb();
			CDItemComponentTable::Instance().LoadFromFdb();
			CDObjectsTable::Instance().LoadFromFdb();
			CDClientManager::ResetTables();
			CDFdb::Retire();
			Game::logger = nullptr;
			std::error_code error;
			std::filesystem::remove_all(m_Dir, error);
		}

		// What master does on a change, then what every server does with the result
		CDClientSnapshot::Result BuildAndSwap(std::optional<uint64_t> currentHash, const std::filesystem::path& previous) {
			auto result = CDClientSnapshot::Build(m_Dir / "res" / "cdclient.fdb", m_Dir / "resServer", m_Dir / "migrations", currentHash, previous);
			if (!result.ok || result.unchanged) return result;
			CDClientDatabase::Reconnect((m_Dir / "resServer" / result.current.sqlite).string());
			CDClientManager::ResetTables();
			EXPECT_TRUE(CDFdb::Open(m_Dir / "resServer" / result.current.fdb));
			EXPECT_TRUE(CDComponentsRegistryTable::Instance().LoadFromFdb());
			return result;
		}

		std::unique_ptr<Logger> m_Logger;
		std::filesystem::path m_Dir;
	};
}

TEST_F(CDClientReloadTest, ResetTablesEmptiesEveryTableButKeepsOldEntriesAlive) {
	EXPECT_GE(CDClientManager::GetTableCount(), 40u);

	auto& items = CDClientManager::GetEntriesMutable<CDItemComponentTable>();
	items[7].baseValue = 1234;
	const auto& held = items[7];

	CDClientManager::ResetTables();
	EXPECT_TRUE(CDClientManager::GetEntriesMutable<CDItemComponentTable>().empty());
	// An entity that took this entry before the reload still reads it
	EXPECT_EQ(held.baseValue, 1234u);
}

TEST_F(CDClientReloadTest, SwapsToANewFdbAndSqlite) {
	FdbTestWriter::WriteFile(m_Dir / "res" / "cdclient.fdb", Registry(10));
	const auto first = BuildAndSwap(std::nullopt, {});
	ASSERT_TRUE(first.ok) << first.error;
	EXPECT_EQ(first.migrations, std::vector<std::string>{ "1_change_lot_5.sql" });
	EXPECT_EQ(Component(1, 1), 10);
	// From the migration, which overrides the fdb row
	EXPECT_EQ(Component(5, 1), 55);

	// Same bytes: nothing to do
	const auto same = CDClientSnapshot::Build(m_Dir / "res" / "cdclient.fdb", m_Dir / "resServer", m_Dir / "migrations", first.hash, {});
	EXPECT_TRUE(same.ok);
	EXPECT_TRUE(same.unchanged);

	// The client's fdb is edited while the server maps its copy
	FdbTestWriter::WriteFile(m_Dir / "res" / "cdclient.fdb", Registry(11));
	const auto second = BuildAndSwap(first.hash, m_Dir / "resServer" / first.current.fdb);
	ASSERT_TRUE(second.ok) << second.error;
	EXPECT_NE(second.hash, first.hash);
	EXPECT_EQ(second.changes, std::vector<std::string>{ "ComponentsRegistry: 2 -> 2 rows (values changed)" });
	EXPECT_EQ(Component(1, 1), 11);
	EXPECT_EQ(Component(5, 1), 55);

	// Both copies are there until the old one is removed
	EXPECT_TRUE(std::filesystem::exists(m_Dir / "resServer" / first.current.fdb));
	EXPECT_TRUE(std::filesystem::exists(m_Dir / "resServer" / second.current.sqlite));
	FdbSnapshot::RemoveOld(m_Dir / "resServer", { second.hash });
	EXPECT_TRUE(std::filesystem::exists(m_Dir / "resServer" / second.current.fdb));
	EXPECT_EQ(Component(1, 1), 11);
}
