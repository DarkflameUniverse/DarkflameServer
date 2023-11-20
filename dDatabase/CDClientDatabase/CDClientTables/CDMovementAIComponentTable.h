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

class CDMovementAIComponentTable : public CDTable<CDMovementAIComponentTable> {
private:
	std::vector<CDMovementAIComponent> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDMovementAIComponent> Query(std::function<bool(CDMovementAIComponent)> predicate);

	// Gets all the entries in the table
	const std::vector<CDMovementAIComponent>& GetEntries() const;
};
