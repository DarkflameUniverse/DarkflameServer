#include "CDClientManager.h"

#include <functional>
#include <memory>
#include <vector>

#include "CDFdb.h"
#include "Logger.h"
#include "CDActivityRewardsTable.h"
#include "CDAnimationsTable.h"
#include "CDBehaviorParameterTable.h"
#include "CDBehaviorTemplateTable.h"
#include "CDClientDatabase.h"
#include "CDComponentsRegistryTable.h"
#include "CDCurrencyTableTable.h"
#include "CDDestructibleComponentTable.h"
#include "CDEmoteTable.h"
#include "CDInventoryComponentTable.h"
#include "CDItemComponentTable.h"
#include "CDItemSetsTable.h"
#include "CDItemSetSkillsTable.h"
#include "CDLevelProgressionLookupTable.h"
#include "CDLootMatrixTable.h"
#include "CDLootTableTable.h"
#include "CDMissionNPCComponentTable.h"
#include "CDMissionTasksTable.h"
#include "CDMissionsTable.h"
#include "CDObjectSkillsTable.h"
#include "CDObjectsTable.h"
#include "CDPhysicsComponentTable.h"
#include "CDRebuildComponentTable.h"
#include "CDCollectibleComponentTable.h"
#include "CDEventGatingTable.h"
#include "CDScriptComponentTable.h"
#include "CDSkillBehaviorTable.h"
#include "CDZoneTableTable.h"
#include "CDTamingBuildPuzzleTable.h"
#include "CDVendorComponentTable.h"
#include "CDActivitiesTable.h"
#include "CDPackageComponentTable.h"
#include "CDProximityMonitorComponentTable.h"
#include "CDMovementAIComponentTable.h"
#include "CDBrickIDTableTable.h"
#include "CDRarityTableTable.h"
#include "CDMissionEmailTable.h"
#include "CDRewardsTable.h"
#include "CDPropertyEntranceComponentTable.h"
#include "CDDeletionRestrictionsTable.h"
#include "CDPropertyTemplateTable.h"
#include "CDModularBuildComponentTable.h"
#include "CDFeatureGatingTable.h"
#include "CDRailActivatorComponent.h"
#include "CDRewardCodesTable.h"
#include "CDPetComponentTable.h"

#ifndef CDCLIENT_CACHE_ALL
// Uncomment this to cache the full cdclient database into memory. This will make the server load faster, but will use more memory.
// A vanilla CDClient takes about 46MB of memory + the regular world data.
// #  define CDCLIENT_CACHE_ALL
#endif // CDCLIENT_CACHE_ALL

#ifdef CDCLIENT_CACHE_ALL
	#define CDCLIENT_DONT_CACHE_TABLE(x) x
#else
	#define CDCLIENT_DONT_CACHE_TABLE(x)
#endif

// Using a macro to reduce repetitive code and issues from copy and paste.
// As a note, ## in a macro is used to concatenate two tokens together.

#define SPECIALIZE_TABLE_STORAGE(table) \
	template<> typename table::StorageType& CDClientManager::GetEntriesMutable<table>() { return table##Entries; };

namespace {
	// Empties each table's storage for Reload, keeping the old entries alive in g_Retired
	std::vector<std::function<void()>>& Resetters() {
		static std::vector<std::function<void()>> resetters;
		return resetters;
	}
	std::vector<std::shared_ptr<void>> g_Retired;

	template<typename Storage>
	bool RegisterReset(Storage& storage) {
		Resetters().push_back([&storage]() {
			g_Retired.push_back(std::make_shared<Storage>(std::move(storage)));
			storage = Storage{};
		});
		return true;
	}
}

#define DEFINE_TABLE_STORAGE(table) namespace { table::StorageType table##Entries; const bool table##Reset = RegisterReset(table##Entries); }; SPECIALIZE_TABLE_STORAGE(table)

DEFINE_TABLE_STORAGE(CDActivityRewardsTable);
DEFINE_TABLE_STORAGE(CDActivitiesTable);
DEFINE_TABLE_STORAGE(CDAnimationsTable);
DEFINE_TABLE_STORAGE(CDBehaviorParameterTable);
DEFINE_TABLE_STORAGE(CDBehaviorTemplateTable);
DEFINE_TABLE_STORAGE(CDBrickIDTableTable);
DEFINE_TABLE_STORAGE(CDComponentsRegistryTable);
DEFINE_TABLE_STORAGE(CDCurrencyTableTable);
DEFINE_TABLE_STORAGE(CDDestructibleComponentTable);
DEFINE_TABLE_STORAGE(CDEmoteTableTable);
DEFINE_TABLE_STORAGE(CDFeatureGatingTable);
DEFINE_TABLE_STORAGE(CDInventoryComponentTable);
DEFINE_TABLE_STORAGE(CDItemComponentTable);
DEFINE_TABLE_STORAGE(CDItemSetSkillsTable);
DEFINE_TABLE_STORAGE(CDItemSetsTable);
DEFINE_TABLE_STORAGE(CDLevelProgressionLookupTable);
DEFINE_TABLE_STORAGE(CDLootMatrixTable);
DEFINE_TABLE_STORAGE(CDLootTableTable);
DEFINE_TABLE_STORAGE(CDMissionEmailTable);
DEFINE_TABLE_STORAGE(CDMissionNPCComponentTable);
DEFINE_TABLE_STORAGE(CDMissionTasksTable);
DEFINE_TABLE_STORAGE(CDMissionsTable);
DEFINE_TABLE_STORAGE(CDMovementAIComponentTable);
DEFINE_TABLE_STORAGE(CDObjectSkillsTable);
DEFINE_TABLE_STORAGE(CDObjectsTable);
DEFINE_TABLE_STORAGE(CDPhysicsComponentTable);
DEFINE_TABLE_STORAGE(CDPackageComponentTable);
DEFINE_TABLE_STORAGE(CDPetComponentTable);
DEFINE_TABLE_STORAGE(CDProximityMonitorComponentTable);
DEFINE_TABLE_STORAGE(CDPropertyEntranceComponentTable);
DEFINE_TABLE_STORAGE(CDDeletionRestrictionsTable);
DEFINE_TABLE_STORAGE(CDPropertyTemplateTable);
DEFINE_TABLE_STORAGE(CDModularBuildComponentTable);
DEFINE_TABLE_STORAGE(CDRailActivatorComponentTable);
DEFINE_TABLE_STORAGE(CDRarityTableTable);
DEFINE_TABLE_STORAGE(CDRebuildComponentTable);
DEFINE_TABLE_STORAGE(CDCollectibleComponentTable);
DEFINE_TABLE_STORAGE(CDEventGatingTable);
DEFINE_TABLE_STORAGE(CDRewardCodesTable);
DEFINE_TABLE_STORAGE(CDRewardsTable);
DEFINE_TABLE_STORAGE(CDScriptComponentTable);
DEFINE_TABLE_STORAGE(CDSkillBehaviorTable);
DEFINE_TABLE_STORAGE(CDTamingBuildPuzzleTable);
DEFINE_TABLE_STORAGE(CDVendorComponentTable);

void CDClientManager::LoadValuesFromDatabase(const std::filesystem::path& fdbPath) {
	if (!CDClientDatabase::isConnected) {
		throw std::runtime_error{ "CDClientDatabase is not connected!" };
	}

	if (!fdbPath.empty()) {
		if (CDFdb::Open(fdbPath)) {
			LOG("Reading CDClient rows by id from %s (%s)", fdbPath.string().c_str(),
				CDFdb::Get()->IsMapped() ? "mapped, shared between processes" : "could not map it, read into memory");
		} else {
			LOG("Could not open %s, reading CDClient from CDServer.sqlite only", fdbPath.string().c_str());
		}
	}

	CDActivityRewardsTable::Instance().LoadValuesFromDatabase();
	CDActivitiesTable::Instance().LoadValuesFromDatabase();
	CDCLIENT_DONT_CACHE_TABLE(CDAnimationsTable::Instance().LoadValuesFromDatabase());
	CDBehaviorParameterTable::Instance().LoadValuesFromDatabase();
	CDBehaviorTemplateTable::Instance().LoadValuesFromDatabase();
	CDBrickIDTableTable::Instance().LoadValuesFromDatabase();
	CDCollectibleComponentTable::Instance().LoadValuesFromDatabase();
	// ComponentsRegistry, ItemComponent and Objects read rows from the fdb when it is open. Without it, the first two are
	// always in memory: every entity and every inventory item asks for its components and item data, and looking up an
	// id not seen yet in the unindexed CDClient scans the whole table (a character holding 3000 different items took a
	// minute to load)
	if (!CDComponentsRegistryTable::Instance().LoadFromFdb()) CDComponentsRegistryTable::Instance().LoadValuesFromDatabase();
	CDCurrencyTableTable::Instance().LoadValuesFromDatabase();
	CDDeletionRestrictionsTable::Instance().LoadValuesFromDatabase();
	CDDestructibleComponentTable::Instance().LoadValuesFromDatabase();
	CDEmoteTableTable::Instance().LoadValuesFromDatabase();
	CDEventGatingTable::Instance().LoadValuesFromDatabase();
	CDFeatureGatingTable::Instance().LoadValuesFromDatabase();
	CDInventoryComponentTable::Instance().LoadValuesFromDatabase();
	if (!CDItemComponentTable::Instance().LoadFromFdb()) CDItemComponentTable::Instance().LoadValuesFromDatabase();
	CDItemSetSkillsTable::Instance().LoadValuesFromDatabase();
	CDItemSetsTable::Instance().LoadValuesFromDatabase();
	CDLevelProgressionLookupTable::Instance().LoadValuesFromDatabase();
	CDCLIENT_DONT_CACHE_TABLE(CDLootMatrixTable::Instance().LoadValuesFromDatabase());
	CDCLIENT_DONT_CACHE_TABLE(CDLootTableTable::Instance().LoadValuesFromDatabase());
	CDMissionEmailTable::Instance().LoadValuesFromDatabase();
	CDMissionNPCComponentTable::Instance().LoadValuesFromDatabase();
	CDMissionTasksTable::Instance().LoadValuesFromDatabase();
	CDMissionsTable::Instance().LoadValuesFromDatabase();
	CDMovementAIComponentTable::Instance().LoadValuesFromDatabase();
	CDObjectSkillsTable::Instance().LoadValuesFromDatabase();
	if (!CDObjectsTable::Instance().LoadFromFdb()) CDCLIENT_DONT_CACHE_TABLE(CDObjectsTable::Instance().LoadValuesFromDatabase());
	CDPhysicsComponentTable::Instance().LoadValuesFromDatabase();
	CDPackageComponentTable::Instance().LoadValuesFromDatabase();
	CDPetComponentTable::Instance().LoadValuesFromDatabase();
	CDProximityMonitorComponentTable::Instance().LoadValuesFromDatabase();
	CDPropertyEntranceComponentTable::Instance().LoadValuesFromDatabase();
	CDPropertyTemplateTable::Instance().LoadValuesFromDatabase();
	CDModularBuildComponentTable::Instance().LoadValuesFromDatabase();
	CDRailActivatorComponentTable::Instance().LoadValuesFromDatabase();
	CDRarityTableTable::Instance().LoadValuesFromDatabase();
	CDRebuildComponentTable::Instance().LoadValuesFromDatabase();
	CDRewardCodesTable::Instance().LoadValuesFromDatabase();
	CDRewardsTable::Instance().LoadValuesFromDatabase();
	CDScriptComponentTable::Instance().LoadValuesFromDatabase();
	CDSkillBehaviorTable::Instance().LoadValuesFromDatabase();
	CDTamingBuildPuzzleTable::Instance().LoadValuesFromDatabase();
	CDVendorComponentTable::Instance().LoadValuesFromDatabase();
	CDZoneTableTable::LoadValuesFromDatabase();
}

void CDClientManager::ResetTables() {
	CDFdb::Retire();
	for (const auto& reset : Resetters()) reset();
	CDZoneTableTable::Reset();
}

void CDClientManager::Reload(const std::filesystem::path& fdbPath) {
	ResetTables();
	LoadValuesFromDatabase(fdbPath);
}

uint32_t CDClientManager::GetTableCount() {
	return static_cast<uint32_t>(Resetters().size()) + 1;
}

void CDClientManager::LoadValuesFromDefaults() {
	LOG("Loading default CDClient tables!");

	CDPetComponentTable::Instance().LoadValuesFromDefaults();
}
