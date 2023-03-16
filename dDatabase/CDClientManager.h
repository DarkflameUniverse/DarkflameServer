#pragma once

#include <type_traits>
#include <unordered_map>

#include "CDTable.h"

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

class CDClientManager {
private:
	static CDClientManager* m_Address;                 //!< The singleton address

	std::unordered_map<std::string, CDTable*> tables;

public:
	static CDClientManager* Instance() {
		if (m_Address == 0) {
			m_Address = new CDClientManager;
		}

		return m_Address;
	}

	void Initialize();

	/**
	 * Fetch a table from CDClient
	 * 
	 * @tparam Table type to fetch
	 * @return A pointer to the requested table.
	 */
	template<typename T>
	T* GetTable() {
		static_assert(std::is_base_of<CDTable, T>::value, "T should inherit from CDTable!");

		auto possibleTable = this->tables.find(T::GetTableName());

		return possibleTable != this->tables.end() ? dynamic_cast<T*>(possibleTable->second) : nullptr;
	}
};
