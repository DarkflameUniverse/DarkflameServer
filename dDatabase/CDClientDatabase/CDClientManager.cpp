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
#include "CDRewardCodesTable.h"

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

CDClientManager::CDClientManager() {
	CDActivityRewardsTable::Instance().LoadValuesFromDatabase();
	CDActivitiesTable::Instance().LoadValuesFromDatabase();
	CDCLIENT_DONT_CACHE_TABLE(CDAnimationsTable::Instance().LoadValuesFromDatabase());
	CDBehaviorParameterTable::Instance().LoadValuesFromDatabase();
	CDBehaviorTemplateTable::Instance().LoadValuesFromDatabase();
	CDBrickIDTableTable::Instance().LoadValuesFromDatabase();
	CDCLIENT_DONT_CACHE_TABLE(CDComponentsRegistryTable::Instance().LoadValuesFromDatabase());
	CDCurrencyTableTable::Instance().LoadValuesFromDatabase();
	CDDestructibleComponentTable::Instance().LoadValuesFromDatabase();
	CDEmoteTableTable::Instance().LoadValuesFromDatabase();
	CDFeatureGatingTable::Instance().LoadValuesFromDatabase();
	CDInventoryComponentTable::Instance().LoadValuesFromDatabase();
	CDCLIENT_DONT_CACHE_TABLE(CDItemComponentTable::Instance().LoadValuesFromDatabase());
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
	CDCLIENT_DONT_CACHE_TABLE(CDObjectsTable::Instance().LoadValuesFromDatabase());
	CDPhysicsComponentTable::Instance().LoadValuesFromDatabase();
	CDPackageComponentTable::Instance().LoadValuesFromDatabase();
	CDProximityMonitorComponentTable::Instance().LoadValuesFromDatabase();
	CDPropertyEntranceComponentTable::Instance().LoadValuesFromDatabase();
	CDPropertyTemplateTable::Instance().LoadValuesFromDatabase();
	CDRailActivatorComponentTable::Instance().LoadValuesFromDatabase();
	CDRarityTableTable::Instance().LoadValuesFromDatabase();
	CDRebuildComponentTable::Instance().LoadValuesFromDatabase();
	CDRewardCodesTable::Instance().LoadValuesFromDatabase();
	CDRewardsTable::Instance().LoadValuesFromDatabase();
	CDScriptComponentTable::Instance().LoadValuesFromDatabase();
	CDSkillBehaviorTable::Instance().LoadValuesFromDatabase();
	CDVendorComponentTable::Instance().LoadValuesFromDatabase();
	CDZoneTableTable::Instance().LoadValuesFromDatabase();
}
