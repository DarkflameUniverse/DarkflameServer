#include "CDFeatureGatingTable.h"

//! Constructor
CDFeatureGatingTable::CDFeatureGatingTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM FeatureGating");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM FeatureGating");
	while (!tableData.eof()) {
		CDFeatureGating entry;
		entry.featureName = tableData.getStringField(0, "");
		entry.major = tableData.getIntField(1, -1);
		entry.current = tableData.getIntField(2, -1);
		entry.minor = tableData.getIntField(3, -1);
		entry.description = tableData.getStringField(4, "");

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDFeatureGatingTable::~CDFeatureGatingTable(void) {}

//! Returns the table's name
std::string CDFeatureGatingTable::GetName(void) const {
	return "FeatureGating";
}

//! Queries the table with a custom "where" clause
std::vector<CDFeatureGating> CDFeatureGatingTable::Query(std::function<bool(CDFeatureGating)> predicate) {

	std::vector<CDFeatureGating> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

bool CDFeatureGatingTable::FeatureUnlocked(const std::string& feature) const {
	for (const auto& entry : entries) {
		if (entry.featureName == feature) {
			return true;
		}
	}

	return false;
}

//! Gets all the entries in the table
std::vector<CDFeatureGating> CDFeatureGatingTable::GetEntries(void) const {
	return this->entries;
}
