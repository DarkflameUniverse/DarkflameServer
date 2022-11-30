#pragma once

// Custom Classes
#include "CDTable.h"

// Tables
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

// C++
#include <type_traits>
#include <unordered_map>

/*!
  \file CDClientManager.hpp
  \brief A manager for the CDClient tables
 */

 //! Manages all data from the CDClient
class CDClientManager {
private:
	static CDClientManager* m_Address;                 //!< The singleton address

	std::unordered_map<std::string, CDTable*> tables;   //!< The tables

public:

	//! The singleton method
	static CDClientManager* Instance() {
		if (m_Address == 0) {
			m_Address = new CDClientManager;
		}

		return m_Address;
	}

	//! Initializes the manager
	void Initialize(void);

	//! Fetches a CDClient table
	/*!
	  This function uses typename T which must be a subclass of CDTable.
	  It returns the class that conforms to the class name

	  \param tableName The table name
	  \return The class or nullptr
	 */
	template<typename T>
	T* GetTable(const std::string& tableName) {
		static_assert(std::is_base_of<CDTable, T>::value, "T should inherit from CDTable!");

		for (auto itr = this->tables.begin(); itr != this->tables.end(); ++itr) {
			if (itr->first == tableName) {
				return dynamic_cast<T*>(itr->second);
			}
		}

		return nullptr;
	}
};
