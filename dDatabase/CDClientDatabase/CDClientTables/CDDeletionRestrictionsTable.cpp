#include "CDDeletionRestrictionsTable.h"
#include "GeneralUtils.h"

//! Constructor
void CDDeletionRestrictionsTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM CurrencyTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM DeletionRestrictions");
	while (!tableData.eof()) {
		CDDeletionRestrictions entry;
		entry.id = tableData.getIntField("id", -1);
		if (entry.id == -1) continue;
		entry.restricted = tableData.getIntField("restricted", -1);
		const std::string raw_ids = tableData.getStringField("ids", "");
		if (!raw_ids.empty()) {
			for (const auto& idstr : GeneralUtils::SplitString(raw_ids, ',')) {
				if (!idstr.empty()) {
					const auto id = GeneralUtils::TryParse<int32_t>(idstr).value_or(-1);
					if (id != -1) entry.ids.push_back(id);
				} 
			}
		}
		entry.checkType = static_cast<eDeletionRestrictionsCheckType>(tableData.getIntField("checkType", 6)); // MAX

		entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDDeletionRestrictions> CDDeletionRestrictionsTable::Query(std::function<bool(CDDeletionRestrictions)> predicate) {
	std::vector<CDDeletionRestrictions> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}
