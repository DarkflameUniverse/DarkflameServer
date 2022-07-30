#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDRebuildComponentTable.hpp
 \brief Contains data for the RebuildComponent table
 */

 //! RebuildComponent Struct
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

//! ObjectSkills table
class CDRebuildComponentTable : public CDTable {
private:
	std::vector<CDRebuildComponent> entries;

public:

	//! Constructor
	CDRebuildComponentTable(void);

	//! Destructor
	~CDRebuildComponentTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDRebuildComponent> Query(std::function<bool(CDRebuildComponent)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDRebuildComponent> GetEntries(void) const;

};

