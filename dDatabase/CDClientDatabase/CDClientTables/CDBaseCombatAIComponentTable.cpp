#include "CDBaseCombatAIComponentTable.h"

namespace {
	// Default entries for fallback
	CDBaseCombatAIComponent defaultEntry{
		.id = 1,
		.aggroRadius = 25.0f,
		.tetherSpeed = 4.0f,
		.pursuitSpeed = 2.0f,
		.softTetherRadius = 25.0f,
		.hardTetherRadius = 100.0f,
	};
}

void CDBaseCombatAIComponentTable::LoadValuesFromDatabase() {
	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::CreatePreppedStmt("SELECT COUNT(*) FROM BaseCombatAIComponent");
	auto tableSizeResult = tableSize.execQuery();
	while (!tableSizeResult.eof()) {
		size = tableSizeResult.getIntField(0, 0);
		tableSizeResult.nextRow();
	}
	tableSizeResult.finalize();

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::CreatePreppedStmt("SELECT * FROM BaseCombatAIComponent");
	auto tableDataResult = tableData.execQuery();

	while (!tableDataResult.eof()) {
		CDBaseCombatAIComponent entry;

		entry.id = tableDataResult.getIntField("id", -1);
		entry.aggroRadius = tableDataResult.getFloatField("aggroRadius", 25.0f);
		entry.tetherSpeed = tableDataResult.getFloatField("tetherSpeed", 4.0f);
		entry.pursuitSpeed = tableDataResult.getFloatField("pursuitSpeed", 2.0f);
		entry.softTetherRadius = tableDataResult.getFloatField("softTetherRadius", 25.0f);
		entry.hardTetherRadius = tableDataResult.getFloatField("hardTetherRadius", 100.0f);

		entries.push_back(entry);
		tableDataResult.nextRow();
	}

	tableData.finalize();
}

void CDBaseCombatAIComponentTable::LoadValuesFromDefaults() {
	auto& entries = GetEntriesMutable();
	entries.clear();
	entries.push_back(defaultEntry);
}

std::vector<CDBaseCombatAIComponent> CDBaseCombatAIComponentTable::Query(std::function<bool(CDBaseCombatAIComponent)> predicate) {
	std::vector<CDBaseCombatAIComponent> data = cpplinq::from(GetEntries())
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const std::vector<CDBaseCombatAIComponent>& CDBaseCombatAIComponentTable::GetEntries() const {
	return CDTable::GetEntries();
}