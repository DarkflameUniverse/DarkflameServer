#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "CDClientDatabase.h"
#include "CDClientManager.h"
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
	void ClearTables() {
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().clear();
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().clear();
		CDClientManager::GetEntriesMutable<CDObjectsTable>().clear();
	}

	int32_t Component(uint32_t id, int32_t type, int32_t defaultValue = -99) {
		return CDComponentsRegistryTable::Instance().GetByIDAndType(id, static_cast<eReplicaComponentType>(type), defaultValue);
	}

	class CDFdbTestBase : public ::testing::Test {
	protected:
		void SetUp() override {
			m_Logger = std::make_unique<Logger>("./testing.log", false, false);
			Game::logger = m_Logger.get();
			ClearTables();
		}

		void TearDown() override {
			// Let go of the fdb before its file is removed; the tables stop using it on their next load
			CDFdb::Close();
			CDComponentsRegistryTable::Instance().LoadFromFdb();
			CDItemComponentTable::Instance().LoadFromFdb();
			CDObjectsTable::Instance().LoadFromFdb();
			ClearTables();
			Game::logger = nullptr;
		}

		std::unique_ptr<Logger> m_Logger;
	};

	// A ComponentsRegistry in both files, where CDServer.sqlite changes, drops and adds rows the way migrations do
	class CDFdbOverlayTest : public CDFdbTestBase {
	protected:
		void SetUp() override {
			CDFdbTestBase::SetUp();
			m_Dir = std::filesystem::temp_directory_path() / ("dlu_cdfdb_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
			std::filesystem::create_directories(m_Dir);

			FdbTestWriter::Table registry;
			registry.name = "ComponentsRegistry";
			registry.columns = { { "id", eSqliteDataType::INT32 }, { "component_type", eSqliteDataType::INT32 }, { "component_id", eSqliteDataType::INT32 } };
			registry.bucketCount = 4;
			// 1, 5 and 9 share bucket 1
			registry.rows = {
				{ Value::Int(1), Value::Int(1), Value::Int(10) },
				{ Value::Int(1), Value::Int(2), Value::Int(20) },
				{ Value::Int(5), Value::Int(1), Value::Int(50) },
				{ Value::Int(9), Value::Int(1), Value::Int(90) },
				{ Value::Int(9), Value::Int(2), Value::Int(91) },
				{ Value::Int(2), Value::Int(1), Value::Int(200) },
				// A repeated (id, type): the last row wins
				{ Value::Int(2), Value::Int(1), Value::Int(201) },
			};
			FdbTestWriter::WriteFile(m_Dir / "cdclient.fdb", FdbTestWriter::Write({ registry }));

			std::filesystem::remove(m_Dir / "CDServer.sqlite");
			CDClientDatabase::Connect((m_Dir / "CDServer.sqlite").string());
			CDClientDatabase::ExecuteDML("CREATE TABLE ComponentsRegistry ('id' int32, 'component_type' int32, 'component_id' int32);");
			CDClientDatabase::ExecuteDML(
				"INSERT INTO ComponentsRegistry VALUES (1, 1, 10), (1, 2, 20), (5, 1, 55), (2, 1, 200), (2, 1, 201), (13, 1, 130);");
		}

		void TearDown() override {
			CDFdbTestBase::TearDown();
			std::error_code error;
			std::filesystem::remove_all(m_Dir, error);
		}

		void ExpectRegistryAnswers() {
			EXPECT_EQ(Component(1, 1), 10);
			EXPECT_EQ(Component(1, 2), 20);
			EXPECT_EQ(Component(1, 3), -99);
			// Changed by CDServer.sqlite
			EXPECT_EQ(Component(5, 1), 55);
			// Dropped by CDServer.sqlite
			EXPECT_EQ(Component(9, 1), -99);
			EXPECT_EQ(Component(9, 2), -99);
			// Added by CDServer.sqlite
			EXPECT_EQ(Component(13, 1), 130);
			EXPECT_EQ(Component(2, 1), 201);
			EXPECT_EQ(Component(3, 1), -99);
		}

		std::filesystem::path m_Dir;
	};
}

TEST_F(CDFdbOverlayTest, FindsTheKeysSqliteChanges) {
	ASSERT_TRUE(CDFdb::Open(m_Dir / "cdclient.fdb"));
	const auto* table = CDFdb::GetTable("ComponentsRegistry");
	ASSERT_NE(table, nullptr);
	const auto changed = CDFdb::FindChangedKeys(*table);
	ASSERT_TRUE(changed.has_value());
	EXPECT_EQ(std::set<int64_t>(changed->begin(), changed->end()), (std::set<int64_t>{ 5, 9, 13 }));

	// Tables that aren't in both files, or whose columns differ, aren't read from the fdb
	EXPECT_EQ(CDFdb::GetTable("Objects"), nullptr);
}

TEST_F(CDFdbOverlayTest, ReadsFromTheFdbWithSqliteChangesOnTop) {
	ASSERT_TRUE(CDFdb::Open(m_Dir / "cdclient.fdb"));
	ASSERT_TRUE(CDComponentsRegistryTable::Instance().LoadFromFdb());

	// Only the changed ids are cached (an id marker plus its rows), not the table
	EXPECT_EQ(CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().size(), 5u);

	ExpectRegistryAnswers();
}

TEST_F(CDFdbOverlayTest, WithoutAnFdbReadsSqlite) {
	EXPECT_FALSE(CDFdb::Open(m_Dir / "missing.fdb"));
	EXPECT_EQ(CDFdb::Get(), nullptr);
	EXPECT_FALSE(CDComponentsRegistryTable::Instance().LoadFromFdb());

	// Lazy SQLite lookups, as before
	ExpectRegistryAnswers();

	// And the whole table in memory, as the server loads it without an fdb
	ClearTables();
	CDComponentsRegistryTable::Instance().LoadValuesFromDatabase();
	ExpectRegistryAnswers();
}

TEST_F(CDFdbOverlayTest, UnmappedFdbReadsTheSame) {
	ASSERT_TRUE(CDFdb::Open(m_Dir / "cdclient.fdb", false));
	EXPECT_FALSE(CDFdb::Get()->IsMapped());
	ASSERT_TRUE(CDComponentsRegistryTable::Instance().LoadFromFdb());
	ExpectRegistryAnswers();
}

namespace {
	std::vector<int64_t> DistinctIds(const std::string& table) {
		std::vector<int64_t> ids;
		auto query = CDClientDatabase::ExecuteQuery("SELECT DISTINCT id FROM " + table + ";");
		while (!query.eof()) {
			ids.push_back(query.getInt64Field(0));
			query.nextRow();
		}
		return ids;
	}

	bool SameFloat(float a, float b) {
		return a == b || (std::isnan(a) && std::isnan(b));
	}

	// Every id of the table in either file, plus some that are in neither
	std::vector<uint32_t> AllIds(const std::string& name) {
		std::set<uint32_t> ids{ 0, 0xFFFFFFFF, 99999999 };
		for (const auto id : DistinctIds(name)) ids.insert(static_cast<uint32_t>(id));
		if (const auto* table = CDFdb::Get() ? CDFdb::Get()->GetTable(name) : nullptr) {
			table->ForEachRow([&](const FdbReader::Row& row) {
				if (const auto key = FdbReader::Table::KeyOf(row)) ids.insert(static_cast<uint32_t>(*key));
			});
		}
		return { ids.begin(), ids.end() };
	}

	struct Snapshot {
		std::vector<int32_t> components;
		std::vector<CDItemComponent> items;
		std::vector<CDObjects> objects;
	};

	Snapshot TakeSnapshot(const std::vector<uint32_t>& registryIds, const std::vector<int32_t>& types,
		const std::vector<uint32_t>& itemIds, const std::vector<uint32_t>& objectIds) {
		Snapshot snapshot;
		for (const auto id : registryIds) {
			for (const auto type : types) snapshot.components.push_back(Component(id, type, -12345));
		}
		for (const auto id : itemIds) snapshot.items.push_back(CDItemComponentTable::Instance().GetItemComponentByID(id));
		for (const auto id : objectIds) snapshot.objects.push_back(CDObjectsTable::Instance().GetByID(id));
		return snapshot;
	}
}

// Every row of the switched tables through the fdb against CDServer.sqlite, when both are around:
// DLU_CLIENT_RES is the client's res folder (with cdclient.fdb), DLU_CDSERVER_SQLITE the converted
// database (default: the source tree's build/resServer/CDServer.sqlite). Opened read-only.
TEST_F(CDFdbTestBase, RealClientParity) {
	const char* res = std::getenv("DLU_CLIENT_RES");
	if (!res) GTEST_SKIP() << "Set DLU_CLIENT_RES to a client res folder to compare against its cdclient.fdb";
	const auto fdbPath = std::filesystem::path(res) / "cdclient.fdb";
	if (!std::filesystem::exists(fdbPath)) GTEST_SKIP() << "No " << fdbPath.string();

	std::string sqlitePath = std::string(PROJECT_SOURCE_DIR) + "/build/resServer/CDServer.sqlite";
	if (const char* env = std::getenv("DLU_CDSERVER_SQLITE")) sqlitePath = env;
	if (!std::filesystem::exists(sqlitePath)) GTEST_SKIP() << "No CDServer.sqlite at " << sqlitePath;

	CDClientDatabase::Connect(sqlitePath);
	CDClientDatabase::ExecuteDML("PRAGMA query_only = ON;"); // never write to the CDClient

	ASSERT_TRUE(CDFdb::Open(fdbPath));
	EXPECT_TRUE(CDFdb::Get()->IsMapped());

	const auto registryIds = AllIds("ComponentsRegistry");
	const auto itemIds = AllIds("ItemComponent");
	const auto objectIds = AllIds("Objects");
	std::vector<int32_t> types{ 999 };
	for (const auto type : [] {
		std::vector<int64_t> found;
		auto query = CDClientDatabase::ExecuteQuery("SELECT DISTINCT component_type FROM ComponentsRegistry;");
		while (!query.eof()) {
			found.push_back(query.getInt64Field(0));
			query.nextRow();
		}
		return found;
	}()) types.push_back(static_cast<int32_t>(type));

	for (const auto* name : { "ComponentsRegistry", "ItemComponent", "Objects" }) {
		const auto* table = CDFdb::GetTable(name);
		ASSERT_NE(table, nullptr) << name;
		const auto changed = CDFdb::FindChangedKeys(*table);
		ASSERT_TRUE(changed.has_value()) << name;
		std::cout << name << ": " << changed->size() << " ids differ between the fdb and CDServer.sqlite" << std::endl;
	}

	// Through the fdb
	ASSERT_TRUE(CDComponentsRegistryTable::Instance().LoadFromFdb());
	ASSERT_TRUE(CDItemComponentTable::Instance().LoadFromFdb());
	ASSERT_TRUE(CDObjectsTable::Instance().LoadFromFdb());
	const auto fromFdb = TakeSnapshot(registryIds, types, itemIds, objectIds);

	// Through CDServer.sqlite, loaded the way the server does without an fdb
	CDFdb::Close();
	ClearTables();
	EXPECT_FALSE(CDComponentsRegistryTable::Instance().LoadFromFdb());
	EXPECT_FALSE(CDItemComponentTable::Instance().LoadFromFdb());
	EXPECT_FALSE(CDObjectsTable::Instance().LoadFromFdb());
	CDComponentsRegistryTable::Instance().LoadValuesFromDatabase();
	CDItemComponentTable::Instance().LoadValuesFromDatabase();
	CDObjectsTable::Instance().LoadValuesFromDatabase();
	const auto fromSqlite = TakeSnapshot(registryIds, types, itemIds, objectIds);

	ASSERT_EQ(fromFdb.components.size(), fromSqlite.components.size());
	uint32_t componentMismatches = 0;
	for (size_t i = 0; i < fromFdb.components.size(); i++) {
		if (fromFdb.components[i] != fromSqlite.components[i]) {
			if (componentMismatches++ < 20) {
				ADD_FAILURE() << "ComponentsRegistry id " << registryIds[i / types.size()] << " type " << types[i % types.size()]
					<< ": fdb " << fromFdb.components[i] << " sqlite " << fromSqlite.components[i];
			}
		}
	}
	EXPECT_EQ(componentMismatches, 0u);

	ASSERT_EQ(fromFdb.items.size(), fromSqlite.items.size());
	uint32_t itemMismatches = 0;
	for (size_t i = 0; i < fromFdb.items.size(); i++) {
		const auto& a = fromFdb.items[i];
		const auto& b = fromSqlite.items[i];
		const bool same = a.id == b.id && a.equipLocation == b.equipLocation && a.baseValue == b.baseValue &&
			a.isKitPiece == b.isKitPiece && a.rarity == b.rarity && a.itemType == b.itemType && a.itemInfo == b.itemInfo &&
			a.inLootTable == b.inLootTable && a.inVendor == b.inVendor && a.isUnique == b.isUnique && a.isBOP == b.isBOP &&
			a.isBOE == b.isBOE && a.reqFlagID == b.reqFlagID && a.reqSpecialtyID == b.reqSpecialtyID &&
			a.reqSpecRank == b.reqSpecRank && a.reqAchievementID == b.reqAchievementID && a.stackSize == b.stackSize &&
			a.color1 == b.color1 && a.decal == b.decal && a.offsetGroupID == b.offsetGroupID && a.buildTypes == b.buildTypes &&
			a.reqPrecondition == b.reqPrecondition && a.animationFlag == b.animationFlag && a.equipEffects == b.equipEffects &&
			a.readyForQA == b.readyForQA && a.itemRating == b.itemRating && a.isTwoHanded == b.isTwoHanded &&
			a.minNumRequired == b.minNumRequired && a.delResIndex == b.delResIndex && a.currencyLOT == b.currencyLOT &&
			a.altCurrencyCost == b.altCurrencyCost && a.subItems == b.subItems && a.noEquipAnimation == b.noEquipAnimation &&
			a.commendationLOT == b.commendationLOT && a.commendationCost == b.commendationCost &&
			a.currencyCosts == b.currencyCosts && a.locStatus == b.locStatus && a.forgeType == b.forgeType &&
			SameFloat(a.SellMultiplier, b.SellMultiplier);
		if (!same && itemMismatches++ < 20) ADD_FAILURE() << "ItemComponent id " << itemIds[i] << " differs";
	}
	EXPECT_EQ(itemMismatches, 0u);

	ASSERT_EQ(fromFdb.objects.size(), fromSqlite.objects.size());
	uint32_t objectMismatches = 0;
	for (size_t i = 0; i < fromFdb.objects.size(); i++) {
		const auto& a = fromFdb.objects[i];
		const auto& b = fromSqlite.objects[i];
		const bool same = a.id == b.id && a.name == b.name && a.type == b.type && SameFloat(a.interactionDistance, b.interactionDistance);
		if (!same && objectMismatches++ < 20) ADD_FAILURE() << "Objects id " << objectIds[i] << " differs: fdb '" << a.name << "' sqlite '" << b.name << "'";
	}
	EXPECT_EQ(objectMismatches, 0u);

	std::cout << "Compared " << registryIds.size() << " ComponentsRegistry ids x " << types.size() << " types, "
		<< itemIds.size() << " ItemComponent ids, " << objectIds.size() << " Objects ids" << std::endl;
}
