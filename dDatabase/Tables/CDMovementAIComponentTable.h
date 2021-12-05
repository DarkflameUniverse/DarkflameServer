#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDMovementAIComponentTable.hpp
 \brief Contains data for the MovementAIComponent table
 */

 //! MovementAIComponent Struct
struct CDMovementAIComponent {
	unsigned int id;
	std::string MovementType;
	float WanderChance;
	float WanderDelayMin;
	float WanderDelayMax;
	float WanderSpeed;
	float WanderRadius;
	std::string attachedPath;
};

//! MovementAIComponent table
class CDMovementAIComponentTable : public CDTable {
private:
	std::vector<CDMovementAIComponent> entries;

public:

	//! Constructor
	CDMovementAIComponentTable(void);

	//! Destructor
	~CDMovementAIComponentTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDMovementAIComponent> Query(std::function<bool(CDMovementAIComponent)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDMovementAIComponent> GetEntries(void) const;

};

