#pragma once

// Custom Classes
#include "CDTable.h"

struct CDFeatureGating {
	std::string featureName;
	int32_t major;
	int32_t current;
	int32_t minor;
	std::string description;

	bool operator>=(const CDFeatureGating& b) const {
		return 	(this->major > b.major) ||
				(this->major == b.major && this->current > b.current) ||
				(this->major == b.major && this->current == b.current && this->minor >= b.minor);
	}
};

class CDFeatureGatingTable : public CDTable<CDFeatureGatingTable, std::vector<CDFeatureGating>> {
public:
	void LoadValuesFromDatabase();

	// Queries the table with a custom "where" clause
	std::vector<CDFeatureGating> Query(std::function<bool(CDFeatureGating)> predicate);

	bool FeatureUnlocked(const CDFeatureGating& feature) const;
};
