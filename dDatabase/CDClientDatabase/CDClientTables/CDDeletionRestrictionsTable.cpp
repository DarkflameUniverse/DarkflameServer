#include "CDDeletionRestrictionsTable.h"

void CDDeletionRestrictionsTable::LoadValuesFromDatabase() {
	auto& entries = GetEntriesMutable();
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT id, restricted, ids, checkType FROM DeletionRestrictions;");
	while (!tableData.eof()) {
		CDDeletionRestriction entry;
		entry.restricted = tableData.getIntField("restricted", 0) != 0;
		entry.ids = tableData.getStringField("ids", "");
		entry.checkType = tableData.getIntField("checkType", 0);
		entries.insert_or_assign(tableData.getIntField("id", -1), std::move(entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

const CDDeletionRestriction* CDDeletionRestrictionsTable::Get(const int32_t id) const {
	const auto& entries = GetEntries();
	const auto it = entries.find(id);
	return it == entries.end() ? nullptr : &it->second;
}
