#pragma once

// Custom Classes
#include "CDTable.h"

struct CDMovementAIComponent {
	uint32_t id;
	std::string MovementType;
	float WanderChance;
	float WanderDelayMin;
	float WanderDelayMax;
	float WanderSpeed;
	float WanderRadius;
	std::string attachedPath;
};

class CDMovementAIComponentTable : public CDTable<CDMovementAIComponentTable, std::vector<CDMovementAIComponent>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDMovementAIComponent> Query(std::function<bool(CDMovementAIComponent)> predicate);
};
