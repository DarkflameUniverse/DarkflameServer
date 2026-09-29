#include "CDEventGatingTable.h"

void CDEventGatingTable::LoadValuesFromDatabase() {
	auto& entries = GetEntriesMutable();
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM EventGating");
	while (!tableData.eof()) {
		CDEventGating entry;
		entry.eventName = tableData.getStringField("eventName", "");
		entry.dateStart = tableData.getInt64Field("date_start", 0);
		entry.dateEnd = tableData.getInt64Field("date_end", 0);
		entries.push_back(entry);
		tableData.nextRow();
	}
	tableData.finalize();
}

bool CDEventGatingTable::IsEventActive(const std::string_view eventName, const int64_t unixTime) const {
	for (const auto& entry : GetEntries()) {
		if (entry.eventName == eventName && entry.dateStart <= unixTime && unixTime <= entry.dateEnd) return true;
	}
	return false;
}
