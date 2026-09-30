#include "GameDependencies.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDFdb.h"
#include "CDItemComponentTable.h"
#include "CDObjectsTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "tinyxml2.h"

#include <gtest/gtest.h>

// Startup and inventory load with and without the fdb. Skipped unless DLU_FDB_BENCH_XML names a
// character save (charxml) to load; run once per mode, each in its own process:
//   DLU_FDB_BENCH_MODE=fdb|sqlite DLU_CLIENT_RES=<client res> DLU_CDSERVER_SQLITE=<CDServer.sqlite>
//   DLU_FDB_BENCH_XML=<charxml> dGameTests --gtest_filter=CDFdbBenchmark.*
namespace {
	// Resident memory in KiB: anonymous (private to the process) and file-backed (shareable)
	struct Rss {
		int64_t anon = -1;
		int64_t file = -1;
	};

	Rss ReadRss() {
		Rss rss;
#ifdef __linux__
		std::ifstream status("/proc/self/status");
		std::string key;
		while (status >> key) {
			if (key == "RssAnon:") status >> rss.anon;
			else if (key == "RssFile:") status >> rss.file;
			else status.ignore(1024, '\n');
		}
#endif
		return rss;
	}

	double MillisecondsSince(std::chrono::steady_clock::time_point start) {
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	}
}

class CDFdbBenchmark : public GameDependenciesTest {
protected:
	void TearDown() override {
		if (!IsSkipped()) TearDownDependencies();
	}
};

TEST_F(CDFdbBenchmark, StartupAndInventoryLoad) {
	const char* xmlPath = std::getenv("DLU_FDB_BENCH_XML");
	const char* sqlitePath = std::getenv("DLU_CDSERVER_SQLITE");
	if (!xmlPath || !sqlitePath) GTEST_SKIP() << "Set DLU_FDB_BENCH_XML and DLU_CDSERVER_SQLITE to run the benchmark";
	const std::string mode = std::getenv("DLU_FDB_BENCH_MODE") ? std::getenv("DLU_FDB_BENCH_MODE") : "fdb";
	std::filesystem::path fdbPath;
	if (mode == "fdb") {
		const char* res = std::getenv("DLU_CLIENT_RES");
		if (!res) GTEST_SKIP() << "Set DLU_CLIENT_RES for the fdb mode";
		fdbPath = std::filesystem::path(res) / "cdclient.fdb";
	}

	SetUpDependencies();
	CDClientDatabase::Connect(sqlitePath);
	CDClientDatabase::ExecuteDML("PRAGMA query_only = ON;");

	const auto before = ReadRss();
	const auto startupStart = std::chrono::steady_clock::now();
	CDClientManager::LoadValuesFromDatabase(fdbPath);
	const auto startupMs = MillisecondsSince(startupStart);
	const auto afterStartup = ReadRss();
	if (mode == "fdb") ASSERT_NE(CDFdb::Get(), nullptr);

	tinyxml2::XMLDocument doc;
	ASSERT_EQ(doc.LoadFile(xmlPath), tinyxml2::XML_SUCCESS);

	CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
	auto entity = std::make_unique<Entity>(1, info);
	auto* inventory = entity->AddComponent<InventoryComponent>(-1);

	const auto loadStart = std::chrono::steady_clock::now();
	inventory->LoadXml(doc);
	const auto loadMs = MillisecondsSince(loadStart);
	const auto afterLoad = ReadRss();

	uint32_t items = 0;
	for (const auto& [type, bag] : inventory->GetInventories()) items += static_cast<uint32_t>(bag->GetItems().size());

	std::cout << "mode=" << mode
		<< " startup_ms=" << startupMs
		<< " inventory_load_ms=" << loadMs << " items=" << items
		<< " rss_anon_kib before=" << before.anon << " after_startup=" << afterStartup.anon << " after_load=" << afterLoad.anon
		<< " rss_file_kib before=" << before.file << " after_startup=" << afterStartup.file << " after_load=" << afterLoad.file
		<< std::endl;

	entity.reset();
}
