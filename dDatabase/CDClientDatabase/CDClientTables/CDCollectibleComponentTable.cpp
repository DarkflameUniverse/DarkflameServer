#include "CDCollectibleComponentTable.h"

void CDCollectibleComponentTable::LoadValuesFromDatabase() {
	auto& entries = GetEntriesMutable();
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM CollectibleComponent");
	while (!tableData.eof()) {
		CDCollectibleComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.requirementMission = tableData.getIntField("requirement_mission", -1);
		entries.insert_or_assign(entry.id, entry);
		tableData.nextRow();
	}
	tableData.finalize();
}

const CDCollectibleComponent* CDCollectibleComponentTable::GetByID(const int32_t id) const {
	const auto& entries = GetEntries();
	const auto it = entries.find(id);
	return it != entries.end() ? &it->second : nullptr;
}
