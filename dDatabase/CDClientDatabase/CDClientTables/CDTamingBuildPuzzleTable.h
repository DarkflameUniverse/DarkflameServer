#pragma once
#include "CDTable.h"

/**
 * Information for the minigame to be completed
 */
struct CDTamingBuildPuzzle {
	UNUSED_COLUMN(uint32_t id = 0;)

	// The LOT of the object that is to be created
	LOT puzzleModelLot = LOT_NULL;

	// The LOT of the NPC
	UNUSED_COLUMN(LOT npcLot = LOT_NULL;)

	// The .lxfml file that contains the bricks required to build the model
	std::string validPieces{};

	// The .lxfml file that contains the bricks NOT required to build the model
	UNUSED_COLUMN(std::string invalidPieces{};)

	// Difficulty value
	UNUSED_COLUMN(int32_t difficulty = 1;)

	// The time limit to complete the build
	float timeLimit = 30.0f;

	// The number of pieces required to complete the minigame
	int32_t numValidPieces = 6;

	// Number of valid pieces
	UNUSED_COLUMN(int32_t totalNumPieces = 16;)

	// Model name
	UNUSED_COLUMN(std::string modelName{};)

	// The .lxfml file that contains the full model
	UNUSED_COLUMN(std::string fullModel{};)

	// The duration of the pet taming minigame
	UNUSED_COLUMN(float duration = 45.0f;)

	// The imagination cost for the tamer to start the minigame
	int32_t imaginationCost = 10;
};

class CDTamingBuildPuzzleTable : public CDTable<CDTamingBuildPuzzleTable, std::unordered_map<LOT, CDTamingBuildPuzzle>> {
public:
	/**
	 * Load values from the CD client database
	*/
	void LoadValuesFromDatabase();

	/**
	 * Gets the pet ability table corresponding to the pet LOT
	 * @returns A pointer to the corresponding table, or nullptr if one cannot be found
	*/
	[[nodiscard]]
	const CDTamingBuildPuzzle* GetByLOT(const LOT lot) const;
};
