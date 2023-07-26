#pragma once

// Custom Classes
#include "CDTable.h"

struct CDRebuildComponent {
	float reset_time;               //!< The reset time
	float complete_time;            //!< The complete time
	unsigned int take_imagination;          //!< The amount of imagination it costs
	bool interruptible;             //!< Whether or not the rebuild is interruptible
	bool self_activator;            //!< Whether or not the rebuild is a rebuild activator itself
	std::string custom_modules;        //!< The custom modules
	unsigned int activityID;                //!< The activity ID
	unsigned int post_imagination_cost;     //!< The post imagination cost
	float time_before_smash;        //!< The time before smash
};

class CDRebuildComponentTable : public CDTable<CDRebuildComponentTable> {
private:
	std::unordered_map<uint32_t, CDRebuildComponent> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	const std::optional<CDRebuildComponent> Get(uint32_t componentId);
};

