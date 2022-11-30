#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

/*!
 \file CDBehaviorTemplateTable.hpp
 \brief Contains data for the BehaviorTemplate table
 */

 //! BehaviorTemplate Entry Struct
struct CDBehaviorTemplate {
	unsigned int behaviorID;                                         //!< The Behavior ID
	unsigned int templateID;                                         //!< The Template ID (LOT)
	unsigned int effectID;                                           //!< The Effect ID attached
	std::unordered_set<std::string>::iterator effectHandle;          //!< The effect handle
};


//! BehaviorTemplate table
class CDBehaviorTemplateTable : public CDTable {
private:
	std::vector<CDBehaviorTemplate> entries;
	std::unordered_map<uint32_t, CDBehaviorTemplate> entriesMappedByBehaviorID;
	std::unordered_set<std::string> m_EffectHandles;
public:

	//! Constructor
	CDBehaviorTemplateTable(void);

	//! Destructor
	~CDBehaviorTemplateTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDBehaviorTemplate> Query(std::function<bool(CDBehaviorTemplate)> predicate);

	//! Gets all the entries in the table
	/*!
	   \return The entries
	 */
	std::vector<CDBehaviorTemplate> GetEntries(void) const;

	const CDBehaviorTemplate GetByBehaviorID(uint32_t behaviorID);
};
