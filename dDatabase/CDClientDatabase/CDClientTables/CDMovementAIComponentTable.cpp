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
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM MovementAIComponent");
	while (!tableData.eof()) {
		auto& entry = entries.emplace_back();
		entry.id = tableData.getIntField("id", -1);
		entry.MovementType = tableData.getStringField("MovementType", "");
		entry.WanderChance = tableData.getFloatField("WanderChance", -1.0f);
		entry.WanderDelayMin = tableData.getFloatField("WanderDelayMin", -1.0f);
		entry.WanderDelayMax = tableData.getFloatField("WanderDelayMax", -1.0f);
		entry.WanderSpeed = tableData.getFloatField("WanderSpeed", -1.0f);
		entry.WanderRadius = tableData.getFloatField("WanderRadius", -1.0f);
		entry.attachedPath = tableData.getStringField("attachedPath", "");

		tableData.nextRow();
	}
}

std::vector<CDMovementAIComponent> CDMovementAIComponentTable::Query(std::function<bool(CDMovementAIComponent)> predicate) {

	std::vector<CDMovementAIComponent> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}
