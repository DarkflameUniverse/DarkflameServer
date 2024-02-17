#pragma once

// Custom Classes
#include "CDTable.h"

struct CDRebuildComponent {
	uint32_t id;                        //!< The component Id
	float reset_time;               //!< The reset time
	float complete_time;            //!< The complete time
	uint32_t take_imagination;          //!< The amount of imagination it costs
	bool interruptible;             //!< Whether or not the rebuild is interruptible
	bool self_activator;            //!< Whether or not the rebuild is a rebuild activator itself
	std::string custom_modules;        //!< The custom modules
	uint32_t activityID;                //!< The activity ID
	uint32_t post_imagination_cost;     //!< The post imagination cost
	float time_before_smash;        //!< The time before smash
};

class CDRebuildComponentTable : public CDTable<CDRebuildComponentTable, std::vector<CDRebuildComponent>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDRebuildComponent> Query(std::function<bool(CDRebuildComponent)> predicate);
};

