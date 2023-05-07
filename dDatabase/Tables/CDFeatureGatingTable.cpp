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
		entry.featureName = tableData.getStringField("featureName", "");
		entry.major = tableData.getIntField("major", -1);
		entry.current = tableData.getIntField("current", -1);
		entry.minor = tableData.getIntField("minor", -1);
		entry.description = tableData.getStringField("description", "");

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

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

std::vector<CDFeatureGating> CDFeatureGatingTable::GetEntries(void) const {
	return this->entries;
}

