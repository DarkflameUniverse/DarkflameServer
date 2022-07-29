#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDMissionNPCComponentTable.hpp
 \brief Contains data for the ObjectSkills table
 */

 //! MissionNPCComponent Struct
struct CDMissionNPCComponent {
	unsigned int id;                //!< The ID
	unsigned int missionID;         //!< The Mission ID
	bool offersMission;     //!< Whether or not this NPC offers a mission
	bool acceptsMission;    //!< Whether or not this NPC accepts a mission
	std::string gate_version;  //!< The gate version
};

//! MissionNPCComponent table
class CDMissionNPCComponentTable : public CDTable {
private:
	std::vector<CDMissionNPCComponent> entries;

public:

	//! Constructor
	CDMissionNPCComponentTable(void);

	//! Destructor
	~CDMissionNPCComponentTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDMissionNPCComponent> Query(std::function<bool(CDMissionNPCComponent)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDMissionNPCComponent> GetEntries(void) const;

};

