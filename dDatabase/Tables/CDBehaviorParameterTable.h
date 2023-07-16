#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

struct CDBehaviorParameter {
	unsigned int behaviorID;											//!< The Behavior ID
	std::unordered_map<std::string, uint32_t>::iterator parameterID;   	//!< The Parameter ID
	float value;            											//!< The value of the behavior template
};

namespace CDBehaviorParameterTable {
	void LoadTableIntoMemory();
	float GetValue(const uint32_t behaviorID, const std::string& name, const float defaultValue = 0.0f);

	std::map<std::string, float> GetParametersByBehaviorID(uint32_t behaviorID);
};
