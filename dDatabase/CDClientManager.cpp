#include "CDClientManager.h"

// Static Variables
CDClientManager* CDClientManager::m_Address = nullptr;

// Allocate at a minimum the number of tables in the cdclient
uint32_t AllocateTableSize() {
	auto result = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) as tableCount FROM sqlite_master WHERE type='table';");
	uint32_t tablesToReserve = !result.eof() ? result.getIntField("tableCount") : 38;
	result.finalize();
	return tablesToReserve;
}

void CDClientManager::Initialize() {
	this->tables.reserve(AllocateTableSize());
	tables.insert(std::make_pair(CDActivityRewardsTable::GetTableName(), new CDActivityRewardsTable()));
	UNUSED(tables.insert(std::make_pair(CDAnimationsTable::GetTableName(), new CDAnimationsTable())));
	tables.insert(std::make_pair(CDBehaviorParameterTable::GetTableName(), new CDBehaviorParameterTable()));
	tables.insert(std::make_pair(CDBehaviorTemplateTable::GetTableName(), new CDBehaviorTemplateTable()));
	tables.insert(std::make_pair(CDComponentsRegistryTable::GetTableName(), new CDComponentsRegistryTable()));
	tables.insert(std::make_pair(CDCurrencyTableTable::GetTableName(), new CDCurrencyTableTable()));
	tables.insert(std::make_pair(CDDestructibleComponentTable::GetTableName(), new CDDestructibleComponentTable()));
	tables.insert(std::make_pair(CDEmoteTableTable::GetTableName(), new CDEmoteTableTable()));
	tables.insert(std::make_pair(CDInventoryComponentTable::GetTableName(), new CDInventoryComponentTable()));
	tables.insert(std::make_pair(CDItemComponentTable::GetTableName(), new CDItemComponentTable()));
	tables.insert(std::make_pair(CDItemSetsTable::GetTableName(), new CDItemSetsTable()));
	tables.insert(std::make_pair(CDItemSetSkillsTable::GetTableName(), new CDItemSetSkillsTable()));
	tables.insert(std::make_pair(CDLevelProgressionLookupTable::GetTableName(), new CDLevelProgressionLookupTable()));
	tables.insert(std::make_pair(CDLootMatrixTable::GetTableName(), new CDLootMatrixTable()));
	tables.insert(std::make_pair(CDLootTableTable::GetTableName(), new CDLootTableTable()));
	tables.insert(std::make_pair(CDMissionNPCComponentTable::GetTableName(), new CDMissionNPCComponentTable()));
	tables.insert(std::make_pair(CDMissionTasksTable::GetTableName(), new CDMissionTasksTable()));
	tables.insert(std::make_pair(CDMissionsTable::GetTableName(), new CDMissionsTable()));
	tables.insert(std::make_pair(CDObjectSkillsTable::GetTableName(), new CDObjectSkillsTable()));
	tables.insert(std::make_pair(CDObjectsTable::GetTableName(), new CDObjectsTable()));
	tables.insert(std::make_pair(CDPhysicsComponentTable::GetTableName(), new CDPhysicsComponentTable()));
	tables.insert(std::make_pair(CDRebuildComponentTable::GetTableName(), new CDRebuildComponentTable()));
	tables.insert(std::make_pair(CDScriptComponentTable::GetTableName(), new CDScriptComponentTable()));
	tables.insert(std::make_pair(CDSkillBehaviorTable::GetTableName(), new CDSkillBehaviorTable()));
	tables.insert(std::make_pair(CDZoneTableTable::GetTableName(), new CDZoneTableTable()));
	tables.insert(std::make_pair(CDVendorComponentTable::GetTableName(), new CDVendorComponentTable()));
	tables.insert(std::make_pair(CDActivitiesTable::GetTableName(), new CDActivitiesTable()));
	tables.insert(std::make_pair(CDPackageComponentTable::GetTableName(), new CDPackageComponentTable()));
	tables.insert(std::make_pair(CDProximityMonitorComponentTable::GetTableName(), new CDProximityMonitorComponentTable()));
	tables.insert(std::make_pair(CDMovementAIComponentTable::GetTableName(), new CDMovementAIComponentTable()));
	tables.insert(std::make_pair(CDBrickIDTableTable::GetTableName(), new CDBrickIDTableTable()));
	tables.insert(std::make_pair(CDRarityTableTable::GetTableName(), new CDRarityTableTable()));
	tables.insert(std::make_pair(CDMissionEmailTable::GetTableName(), new CDMissionEmailTable()));
	tables.insert(std::make_pair(CDRewardsTable::GetTableName(), new CDRewardsTable()));
	tables.insert(std::make_pair(CDPropertyEntranceComponentTable::GetTableName(), new CDPropertyEntranceComponentTable()));
	tables.insert(std::make_pair(CDPropertyTemplateTable::GetTableName(), new CDPropertyTemplateTable()));
	tables.insert(std::make_pair(CDFeatureGatingTable::GetTableName(), new CDFeatureGatingTable()));
	tables.insert(std::make_pair(CDRailActivatorComponentTable::GetTableName(), new CDRailActivatorComponentTable()));
}
