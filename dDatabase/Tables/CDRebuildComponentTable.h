#pragma once

// Custom Classes
#include "CDTable.h"

struct CDRebuildComponent {
	unsigned int id;                        //!< The component Id
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
	std::vector<CDRebuildComponent> entries;

public:
	CDRebuildComponentTable();
	// Queries the table with a custom "where" clause
	std::vector<CDRebuildComponent> Query(std::function<bool(CDRebuildComponent)> predicate);

	std::vector<CDRebuildComponent> GetEntries() const;
};

