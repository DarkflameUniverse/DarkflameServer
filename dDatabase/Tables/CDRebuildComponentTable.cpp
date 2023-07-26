#include "CDRebuildComponentTable.h"

void CDRebuildComponentTable::LoadValuesFromDatabase() {
	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM RebuildComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM RebuildComponent order by id");
	while (!tableData.eof()) {
		CDRebuildComponent entry;
		uint32_t id = tableData.getIntField("id", -1);
		entry.reset_time = tableData.getFloatField("reset_time", -1.0f);
		entry.complete_time = tableData.getFloatField("complete_time", -1.0f);
		entry.take_imagination = tableData.getIntField("take_imagination", -1);
		entry.interruptible = tableData.getIntField("interruptible", -1) == 1 ? true : false;
		entry.self_activator = tableData.getIntField("self_activator", -1) == 1 ? true : false;
		entry.custom_modules = tableData.getStringField("custom_modules", "");
		entry.activityID = tableData.getIntField("activityID", -1);
		entry.post_imagination_cost = tableData.getIntField("post_imagination_cost", -1);
		entry.time_before_smash = tableData.getFloatField("time_before_smash", -1.0f);

		this->entries.push_back(std::make_pair(id, entry));
		tableData.nextRow();
	}
}

const std::optional<CDRebuildComponent> CDRebuildComponentTable::Get(uint32_t componentId) {
	auto result = std::lower_bound(this->entries.begin(), this->entries.end(), componentId, [](const auto& entry, const auto& componentId) {
		return entry.first < componentId;
	});
	return result == this->entries.end() ? std::nullopt : std::make_optional(result->second);
}
