#include "CDMovementAIComponentTable.h"

//! Constructor
CDMovementAIComponentTable::CDMovementAIComponentTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
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
		entry.id = tableData.getIntField(0, -1);
		entry.MovementType = tableData.getStringField(1, "");
		entry.WanderChance = tableData.getFloatField(2, -1.0f);
		entry.WanderDelayMin = tableData.getFloatField(3, -1.0f);
		entry.WanderDelayMax = tableData.getFloatField(4, -1.0f);
		entry.WanderSpeed = tableData.getFloatField(5, -1.0f);
		entry.WanderRadius = tableData.getFloatField(6, -1.0f);
		entry.attachedPath = tableData.getStringField(7, "");

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDMovementAIComponentTable::~CDMovementAIComponentTable(void) {}

//! Returns the table's name
std::string CDMovementAIComponentTable::GetName(void) const {
	return "MovementAIComponent";
}

//! Queries the table with a custom "where" clause
std::vector<CDMovementAIComponent> CDMovementAIComponentTable::Query(std::function<bool(CDMovementAIComponent)> predicate) {

	std::vector<CDMovementAIComponent> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDMovementAIComponent> CDMovementAIComponentTable::GetEntries(void) const {
	return this->entries;
}
