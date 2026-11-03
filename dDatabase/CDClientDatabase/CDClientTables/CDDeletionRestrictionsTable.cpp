#include "CDDeletionRestrictionsTable.h"
#include "GeneralUtils.h"
#include "eDeletionRestrictionsCheckType.h"

void CDDeletionRestrictionsTable::LoadValuesFromDatabase() {
	auto& entries = GetEntriesMutable();

	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM DeletionRestrictions");
	while (!tableData.eof()) {
		CDDeletionRestriction entry;
		entry.id = tableData.getIntField("id", 0);
		entry.restricted = tableData.getIntField("restricted", 0) != 0;
		for (auto id : GeneralUtils::SplitString(tableData.getStringField("ids", ""), ',')) {
			// The ids are written with spaces around them
			id.erase(0, id.find_first_not_of(" \t"));
			id.erase(id.find_last_not_of(" \t") + 1);
			const auto parsed = GeneralUtils::TryParse<uint32_t>(id);
			if (parsed) entry.ids.push_back(*parsed);
		}
		entry.checkType = static_cast<eDeletionRestrictionsCheckType>(tableData.getIntField("checkType", static_cast<int>(eDeletionRestrictionsCheckType::MAX)));

		entries.insert_or_assign(entry.id, entry);
		tableData.nextRow();
	}
}

std::optional<CDDeletionRestriction> CDDeletionRestrictionsTable::GetByID(const uint32_t id) const {
	const auto& entries = GetEntries();
	const auto it = entries.find(id);
	if (it == entries.end()) return std::nullopt;
	return it->second;
}
