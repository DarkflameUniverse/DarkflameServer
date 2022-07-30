#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDScriptComponentTable.hpp
 \brief Contains data for the ScriptComponent table
 */

 //! ScriptComponent Struct
struct CDScriptComponent {
	unsigned int id;                        //!< The component ID
	std::string script_name;           //!< The script name
	std::string client_script_name;    //!< The client script name
};

//! ObjectSkills table
class CDScriptComponentTable : public CDTable {
private:
	std::map<unsigned int, CDScriptComponent> entries;
	CDScriptComponent m_ToReturnWhenNoneFound;

public:
	//! Gets an entry by ID
	const CDScriptComponent& GetByID(unsigned int id);

	//! Constructor
	CDScriptComponentTable(void);

	//! Destructor
	~CDScriptComponentTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	 \param predicate The predicate
	 */

};

