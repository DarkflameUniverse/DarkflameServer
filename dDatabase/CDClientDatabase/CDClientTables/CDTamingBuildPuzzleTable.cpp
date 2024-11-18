#include "CDTamingBuildPuzzleTable.h"

void CDTamingBuildPuzzleTable::LoadValuesFromDatabase() {
	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM TamingBuildPuzzles");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);
		tableSize.nextRow();
	}

	// Reserve the size
	auto& entries = GetEntriesMutable();
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM TamingBuildPuzzles");
	while (!tableData.eof()) {
		const auto lot = static_cast<LOT>(tableData.getIntField("NPCLot", LOT_NULL));
		entries.emplace(lot, CDTamingBuildPuzzle{
			.puzzleModelLot = lot,
			.validPieces{ tableData.getStringField("ValidPiecesLXF") },
			.timeLimit = static_cast<float>(tableData.getFloatField("Timelimit", 30.0f)),
			.numValidPieces = tableData.getIntField("NumValidPieces", 6),
			.imaginationCost = tableData.getIntField("imagCostPerBuild", 10)
		});
		tableData.nextRow();
	}
}

const CDTamingBuildPuzzle* CDTamingBuildPuzzleTable::GetByLOT(const LOT lot) const {
	const auto& entries = GetEntries();
	const auto itr = entries.find(lot);
	return itr != entries.cend() ? &itr->second : nullptr;
}
