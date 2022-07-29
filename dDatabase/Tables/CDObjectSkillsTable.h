#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDObjectSkillsTable.hpp
 \brief Contains data for the ObjectSkills table
 */

 //! ObjectSkills Struct
struct CDObjectSkills {
	unsigned int objectTemplate;        //!< The LOT of the item
	unsigned int skillID;               //!< The Skill ID of the object
	unsigned int castOnType;            //!< ???
	unsigned int AICombatWeight;        //!< ???
};

//! ObjectSkills table
class CDObjectSkillsTable : public CDTable {
private:
	std::vector<CDObjectSkills> entries;

public:

	//! Constructor
	CDObjectSkillsTable(void);

	//! Destructor
	~CDObjectSkillsTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDObjectSkills> Query(std::function<bool(CDObjectSkills)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDObjectSkills> GetEntries(void) const;

};

