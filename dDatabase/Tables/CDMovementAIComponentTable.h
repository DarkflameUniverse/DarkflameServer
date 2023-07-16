#pragma once

// Custom Classes
#include "CDTable.h"

struct CDMovementAIComponent {
	unsigned int id;
	std::string MovementType;
	float WanderChance;
	float WanderDelayMin;
	float WanderDelayMax;
	float WanderSpeed;
	float WanderRadius;
	std::string attachedPath;
};

namespace CDMovementAIComponentTable {
private:
	std::vector<CDMovementAIComponent> entries;

public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDMovementAIComponent> Query(std::function<bool(CDMovementAIComponent)> predicate);

	// Gets all the entries in the table
	std::vector<CDMovementAIComponent> GetEntries(void) const;
};
