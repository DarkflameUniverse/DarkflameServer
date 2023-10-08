#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

class CDBehaviorParameterTable : public CDTable<CDBehaviorParameterTable> {
private:
	typedef uint64_t BehaviorParameterHash;
	typedef float BehaviorParameterValue;
	std::unordered_map<BehaviorParameterHash, BehaviorParameterValue> m_Entries;
	std::unordered_map<std::string, uint32_t> m_ParametersList;
public:
	void LoadValuesFromDatabase();

	float GetValue(const uint32_t behaviorID, const std::string& name, const float defaultValue = 0);

	std::map<std::string, float> GetParametersByBehaviorID(uint32_t behaviorID);
};
