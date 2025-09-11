#include "CDBaseCombatAIComponentTable.h"

namespace {
	// Default entries for fallback
	CDBaseCombatAIComponent defaultEntry{
		.id = 1,
		.behaviorType = 0,
		.combatRoundLength = 5.0f,
		.combatRole = 0,
		.minRoundLength = 3.0f,
		.maxRoundLength = 8.0f,
		.tetherSpeed = 4.0f,
		.pursuitSpeed = 2.0f,
		.combatStartDelay = 0.5f,
		.softTetherRadius = 25.0f,
		.hardTetherRadius = 100.0f,
		.spawnTimer = 0.0f,
		.tetherEffectID = 0,
		.ignoreMediator = false,
		.aggroRadius = 25.0f,
		.ignoreStatReset = false,
		.ignoreParent = false,
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
		entry.behaviorType = tableDataResult.getIntField("behaviorType", 0);
		entry.combatRoundLength = tableDataResult.getFloatField("combatRoundLength", 5.0f);
		entry.combatRole = tableDataResult.getIntField("combatRole", 0);
		entry.minRoundLength = tableDataResult.getFloatField("minRoundLength", 3.0f);
		entry.maxRoundLength = tableDataResult.getFloatField("maxRoundLength", 8.0f);
		entry.tetherSpeed = tableDataResult.getFloatField("tetherSpeed", 4.0f);
		entry.pursuitSpeed = tableDataResult.getFloatField("pursuitSpeed", 2.0f);
		entry.combatStartDelay = tableDataResult.getFloatField("combatStartDelay", 0.5f);
		entry.softTetherRadius = tableDataResult.getFloatField("softTetherRadius", 25.0f);
		entry.hardTetherRadius = tableDataResult.getFloatField("hardTetherRadius", 100.0f);
		entry.spawnTimer = tableDataResult.getFloatField("spawnTimer", 0.0f);
		entry.tetherEffectID = tableDataResult.getIntField("tetherEffectID", 0);
		entry.ignoreMediator = tableDataResult.getIntField("ignoreMediator", 0) != 0;
		entry.aggroRadius = tableDataResult.getFloatField("aggroRadius", 25.0f);
		entry.ignoreStatReset = tableDataResult.getIntField("ignoreStatReset", 0) != 0;
		entry.ignoreParent = tableDataResult.getIntField("ignoreParent", 0) != 0;

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