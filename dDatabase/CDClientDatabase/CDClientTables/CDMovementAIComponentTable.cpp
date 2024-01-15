#include "CDMovementAIComponentTable.h"

void CDMovementAIComponentTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM MovementAIComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM MovementAIComponent");
	while (!tableData.eof()) {
		CDMovementAIComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.MovementType = tableData.getStringField("MovementType", "");
		entry.WanderChance = tableData.getFloatField("WanderChance", -1.0f);
		entry.WanderDelayMin = tableData.getFloatField("WanderDelayMin", -1.0f);
		entry.WanderDelayMax = tableData.getFloatField("WanderDelayMax", -1.0f);
		entry.WanderSpeed = tableData.getFloatField("WanderSpeed", -1.0f);
		entry.WanderRadius = tableData.getFloatField("WanderRadius", -1.0f);
		entry.attachedPath = tableData.getStringField("attachedPath", "");

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDMovementAIComponent> CDMovementAIComponentTable::Query(std::function<bool(CDMovementAIComponent)> predicate) {

	std::vector<CDMovementAIComponent> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const std::vector<CDMovementAIComponent>& CDMovementAIComponentTable::GetEntries(void) const {
	return this->entries;
}

