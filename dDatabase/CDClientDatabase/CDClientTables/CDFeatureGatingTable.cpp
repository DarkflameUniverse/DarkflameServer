#include "CDFeatureGatingTable.h"

void CDFeatureGatingTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM FeatureGating");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM FeatureGating");
	while (!tableData.eof()) {
		CDFeatureGating entry;
		entry.featureName = tableData.getStringField("featureName", "");
		entry.major = tableData.getIntField("major", -1);
		entry.current = tableData.getIntField("current", -1);
		entry.minor = tableData.getIntField("minor", -1);
		entry.description = tableData.getStringField("description", "");

		entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDFeatureGating> CDFeatureGatingTable::Query(std::function<bool(CDFeatureGating)> predicate) {

	auto& entries = GetEntriesMutable();
	std::vector<CDFeatureGating> data = cpplinq::from(entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

bool CDFeatureGatingTable::FeatureUnlocked(const CDFeatureGating& feature) const {
	auto& entries = GetEntriesMutable();
	for (const auto& entry : entries) {
		if (entry.featureName == feature.featureName && feature >= entry) {
			return true;
		}
	}

	return false;
}
