#include "CDTamingBuildPuzzleTable.h"

const CDTamingBuildPuzzle CDTamingBuildPuzzleTable::defaultEntry = CDTamingBuildPuzzle{};

void CDTamingBuildPuzzleTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM TamingBuildPuzzles");
	auto& entries = GetEntriesMutable();

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
	tableData.finalize();
}

const CDTamingBuildPuzzle& CDTamingBuildPuzzleTable::GetByLOT(const LOT lot) {
	const auto& entries = GetEntries();
	const auto itr = entries.find(lot);
	return itr != entries.cend() ? itr->second : defaultEntry;
}
