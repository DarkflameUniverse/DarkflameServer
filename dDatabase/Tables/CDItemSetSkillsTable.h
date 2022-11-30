#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDItemSetSkillsTable.hpp
 \brief Contains data for the ItemSetSkills table
 */

 //! ZoneTable Struct
struct CDItemSetSkills {
	unsigned int SkillSetID;            //!< The skill set ID
	unsigned int SkillID;               //!< The skill ID
	unsigned int SkillCastType;         //!< The skill cast type
};

//! ItemSets table
class CDItemSetSkillsTable : public CDTable {
private:
	std::vector<CDItemSetSkills> entries;

public:

	//! Constructor
	CDItemSetSkillsTable(void);

	//! Destructor
	~CDItemSetSkillsTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	 \param predicate The predicate
	 */
	std::vector<CDItemSetSkills> Query(std::function<bool(CDItemSetSkills)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDItemSetSkills> GetEntries(void) const;

	std::vector<CDItemSetSkills> GetBySkillID(unsigned int SkillSetID);

};

