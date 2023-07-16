#include "CDActivityRewardsTable.h"

namespace {
	std::vector<CDActivityRewards> entries;
};

void CDActivityRewardsTable::LoadTableIntoMemory() {
	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ActivityRewards");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ActivityRewards");
	while (!tableData.eof()) {
		CDActivityRewards entry;
		entry.objectTemplate = tableData.getIntField("objectTemplate", -1);
		entry.ActivityRewardIndex = tableData.getIntField("ActivityRewardIndex", -1);
		entry.activityRating = tableData.getIntField("activityRating", -1);
		entry.LootMatrixIndex = tableData.getIntField("LootMatrixIndex", -1);
		entry.CurrencyIndex = tableData.getIntField("CurrencyIndex", -1);
		entry.ChallengeRating = tableData.getIntField("ChallengeRating", -1);
		entry.description = tableData.getStringField("description", "");

		entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDActivityRewards> CDActivityRewardsTable::Query(std::function<bool(CDActivityRewards)> predicate) {

	std::vector<CDActivityRewards> data = cpplinq::from(entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}
