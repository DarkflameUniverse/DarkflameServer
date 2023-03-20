#pragma once

// Custom Classes
#include "CDTable.h"

struct CDActivityRewards {
	unsigned int objectTemplate;
	unsigned int ActivityRewardIndex;
	int activityRating;
	unsigned int LootMatrixIndex;
	unsigned int CurrencyIndex;
	unsigned int ChallengeRating;
	std::string description;
};

class CDActivityRewardsTable : public CDTable<CDActivityRewardsTable> {
private:
	std::vector<CDActivityRewards> entries;

public:
	CDActivityRewardsTable();
	// Queries the table with a custom "where" clause
	std::vector<CDActivityRewards> Query(std::function<bool(CDActivityRewards)> predicate);

	std::vector<CDActivityRewards> GetEntries(void) const;

};
