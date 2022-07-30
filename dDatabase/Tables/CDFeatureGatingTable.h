#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDFeatureGatingTable.hpp
 */

 //! ItemComponent Struct
struct CDFeatureGating {
	std::string featureName;
	int32_t major;
	int32_t current;
	int32_t minor;
	std::string description;
};

//! ItemComponent table
class CDFeatureGatingTable : public CDTable {
private:
	std::vector<CDFeatureGating> entries;

public:

	//! Constructor
	CDFeatureGatingTable(void);

	//! Destructor
	~CDFeatureGatingTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDFeatureGating> Query(std::function<bool(CDFeatureGating)> predicate);

	bool FeatureUnlocked(const std::string& feature) const;

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDFeatureGating> GetEntries(void) const;

};
