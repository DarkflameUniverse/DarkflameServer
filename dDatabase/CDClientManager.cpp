#include "CDClientManager.h"

// Static Variables
CDClientManager* CDClientManager::m_Address = nullptr;

//! Initializes the manager
void CDClientManager::Initialize(void) {
	tables.insert(std::make_pair("ActivityRewards", new CDActivityRewardsTable()));
	UNUSED(tables.insert(std::make_pair("Animations", new CDAnimationsTable())));
	tables.insert(std::make_pair("BehaviorParameter", new CDBehaviorParameterTable()));
	tables.insert(std::make_pair("BehaviorTemplate", new CDBehaviorTemplateTable()));
	tables.insert(std::make_pair("ComponentsRegistry", new CDComponentsRegistryTable()));
	tables.insert(std::make_pair("CurrencyTable", new CDCurrencyTableTable()));
	tables.insert(std::make_pair("DestructibleComponent", new CDDestructibleComponentTable()));
	tables.insert(std::make_pair("EmoteTable", new CDEmoteTableTable()));
	tables.insert(std::make_pair("InventoryComponent", new CDInventoryComponentTable()));
	tables.insert(std::make_pair("ItemComponent", new CDItemComponentTable()));
	tables.insert(std::make_pair("ItemSets", new CDItemSetsTable()));
	tables.insert(std::make_pair("ItemSetSkills", new CDItemSetSkillsTable()));
	tables.insert(std::make_pair("LevelProgressionLookup", new CDLevelProgressionLookupTable()));
	tables.insert(std::make_pair("LootMatrix", new CDLootMatrixTable()));
	tables.insert(std::make_pair("LootTable", new CDLootTableTable()));
	tables.insert(std::make_pair("MissionNPCComponent", new CDMissionNPCComponentTable()));
	tables.insert(std::make_pair("MissionTasks", new CDMissionTasksTable()));
	tables.insert(std::make_pair("Missions", new CDMissionsTable()));
	tables.insert(std::make_pair("ObjectSkills", new CDObjectSkillsTable()));
	tables.insert(std::make_pair("Objects", new CDObjectsTable()));
	tables.insert(std::make_pair("PhysicsComponent", new CDPhysicsComponentTable()));
	tables.insert(std::make_pair("RebuildComponent", new CDRebuildComponentTable()));
	tables.insert(std::make_pair("ScriptComponent", new CDScriptComponentTable()));
	tables.insert(std::make_pair("SkillBehavior", new CDSkillBehaviorTable()));
	tables.insert(std::make_pair("ZoneTable", new CDZoneTableTable()));
	tables.insert(std::make_pair("VendorComponent", new CDVendorComponentTable()));
	tables.insert(std::make_pair("Activities", new CDActivitiesTable()));
	tables.insert(std::make_pair("PackageComponent", new CDPackageComponentTable()));
	tables.insert(std::make_pair("ProximityMonitorComponent", new CDProximityMonitorComponentTable()));
	tables.insert(std::make_pair("MovementAIComponent", new CDMovementAIComponentTable()));
	tables.insert(std::make_pair("BrickIDTable", new CDBrickIDTableTable()));
	tables.insert(std::make_pair("RarityTable", new CDRarityTableTable()));
	tables.insert(std::make_pair("MissionEmail", new CDMissionEmailTable()));
	tables.insert(std::make_pair("Rewards", new CDRewardsTable()));
	tables.insert(std::make_pair("PropertyEntranceComponent", new CDPropertyEntranceComponentTable()));
	tables.insert(std::make_pair("PropertyTemplate", new CDPropertyTemplateTable()));
	tables.insert(std::make_pair("FeatureGating", new CDFeatureGatingTable()));
	tables.insert(std::make_pair("RailActivatorComponent", new CDRailActivatorComponentTable()));
}
