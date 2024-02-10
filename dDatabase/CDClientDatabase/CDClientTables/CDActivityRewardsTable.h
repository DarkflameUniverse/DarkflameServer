#pragma once

// Custom Classes
#include "CDTable.h"

struct CDActivityRewards {
	uint32_t objectTemplate;        //!< The object template (?)
	uint32_t ActivityRewardIndex;   //!< The activity reward index
	int32_t activityRating;                 //!< The activity rating
	uint32_t LootMatrixIndex;       //!< The loot matrix index
	uint32_t CurrencyIndex;         //!< The currency index
	uint32_t ChallengeRating;       //!< The challenge rating
	std::string description;            //!< The description
};

class CDActivityRewardsTable : public CDTable<CDActivityRewardsTable, std::vector<CDActivityRewards>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDActivityRewards> Query(std::function<bool(CDActivityRewards)> predicate);
};
