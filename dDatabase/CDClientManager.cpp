#include "CDClientManager.h"
#include "CDActivityRewardsTable.h"
#include "CDAnimationsTable.h"
#include "CDBehaviorParameterTable.h"
#include "CDBehaviorTemplateTable.h"
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
#include "CDScriptComponentTable.h"
#include "CDSkillBehaviorTable.h"
#include "CDZoneTableTable.h"
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
#include "CDPropertyTemplateTable.h"
#include "CDFeatureGatingTable.h"
#include "CDRailActivatorComponent.h"

CDClientManager::CDClientManager() {
	CDActivityRewardsTable::Instance();
	UNUSED(CDAnimationsTable::Instance());
	CDBehaviorParameterTable::Instance();
	CDBehaviorTemplateTable::Instance();
	CDComponentsRegistryTable::Instance();
	CDCurrencyTableTable::Instance();
	CDDestructibleComponentTable::Instance();
	CDEmoteTableTable::Instance();
	CDInventoryComponentTable::Instance();
	CDItemComponentTable::Instance();
	CDItemSetsTable::Instance();
	CDItemSetSkillsTable::Instance();
	CDLevelProgressionLookupTable::Instance();
	CDLootMatrixTable::Instance();
	CDLootTableTable::Instance();
	CDMissionNPCComponentTable::Instance();
	CDMissionTasksTable::Instance();
	CDMissionsTable::Instance();
	CDObjectSkillsTable::Instance();
	CDObjectsTable::Instance();
	CDPhysicsComponentTable::Instance();
	CDRebuildComponentTable::Instance();
	CDScriptComponentTable::Instance();
	CDSkillBehaviorTable::Instance();
	CDZoneTableTable::Instance();
	CDVendorComponentTable::Instance();
	CDActivitiesTable::Instance();
	CDPackageComponentTable::Instance();
	CDProximityMonitorComponentTable::Instance();
	CDMovementAIComponentTable::Instance();
	CDBrickIDTableTable::Instance();
	CDRarityTableTable::Instance();
	CDMissionEmailTable::Instance();
	CDRewardsTable::Instance();
	CDPropertyEntranceComponentTable::Instance();
	CDPropertyTemplateTable::Instance();
	CDFeatureGatingTable::Instance();
	CDRailActivatorComponentTable::Instance();
	CDFaceItemComponentTable::Instance();
}
