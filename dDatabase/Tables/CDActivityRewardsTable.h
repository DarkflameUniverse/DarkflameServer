#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDActivityRewardsTable.hpp
 \brief Contains data for the ActivityRewards table
 */

 //! ActivityRewards Entry Struct
struct CDActivityRewards {
	unsigned int objectTemplate;        //!< The object template (?)
	unsigned int ActivityRewardIndex;   //!< The activity reward index
	int activityRating;                 //!< The activity rating
	unsigned int LootMatrixIndex;       //!< The loot matrix index
	unsigned int CurrencyIndex;         //!< The currency index
	unsigned int ChallengeRating;       //!< The challenge rating
	std::string description;            //!< The description
};


//! ActivityRewards table
class CDActivityRewardsTable : public CDTable {
private:
	std::vector<CDActivityRewards> entries;

public:

	//! Constructor
	CDActivityRewardsTable(void);

	//! Destructor
	~CDActivityRewardsTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDActivityRewards> Query(std::function<bool(CDActivityRewards)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDActivityRewards> GetEntries(void) const;

};
