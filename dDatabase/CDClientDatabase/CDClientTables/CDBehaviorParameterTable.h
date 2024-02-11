#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

typedef uint64_t BehaviorParameterHash;
typedef float BehaviorParameterValue;

class CDBehaviorParameterTable : public CDTable<CDBehaviorParameterTable, std::unordered_map<BehaviorParameterHash, BehaviorParameterValue>> {
public:
	void LoadValuesFromDatabase();

	float GetValue(const uint32_t behaviorID, const std::string& name, const float defaultValue = 0);

	std::map<std::string, float> GetParametersByBehaviorID(uint32_t behaviorID);
};
