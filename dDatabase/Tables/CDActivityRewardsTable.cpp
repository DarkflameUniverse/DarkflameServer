#include "CDActivityRewardsTable.h"

//! Constructor
CDActivityRewardsTable::CDActivityRewardsTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ActivityRewards");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ActivityRewards");
	while (!tableData.eof()) {
		CDActivityRewards entry;
		entry.objectTemplate = tableData.getIntField(0, -1);
		entry.ActivityRewardIndex = tableData.getIntField(1, -1);
		entry.activityRating = tableData.getIntField(2, -1);
		entry.LootMatrixIndex = tableData.getIntField(3, -1);
		entry.CurrencyIndex = tableData.getIntField(4, -1);
		entry.ChallengeRating = tableData.getIntField(5, -1);
		entry.description = tableData.getStringField(6, "");

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDActivityRewardsTable::~CDActivityRewardsTable(void) {}

//! Returns the table's name
std::string CDActivityRewardsTable::GetName(void) const {
	return "ActivityRewards";
}

//! Queries the table with a custom "where" clause
std::vector<CDActivityRewards> CDActivityRewardsTable::Query(std::function<bool(CDActivityRewards)> predicate) {

	std::vector<CDActivityRewards> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDActivityRewards> CDActivityRewardsTable::GetEntries(void) const {
	return this->entries;
}
