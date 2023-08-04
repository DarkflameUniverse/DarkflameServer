#include "CDBehaviorParameterTable.h"
#include "GeneralUtils.h"

uint64_t GetKey(const uint32_t behaviorID, const uint32_t parameterID) {
	uint64_t key = behaviorID;
	key <<= 31U;
	key |= parameterID;

	return key;
}

CDBehaviorParameterTable::CDBehaviorParameterTable() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM BehaviorParameter");
	while (!tableData.eof()) {
		uint32_t behaviorID = tableData.getIntField("behaviorID", -1);
		auto candidateStringToAdd = std::string(tableData.getStringField("parameterID", ""));
		auto parameter = m_ParametersList.find(candidateStringToAdd);
		uint32_t parameterId;
		if (parameter != m_ParametersList.end()) {
			parameterId = parameter->second;
		} else {
			parameterId = m_ParametersList.insert(std::make_pair(candidateStringToAdd, m_ParametersList.size())).first->second;
		}
		uint64_t hash = GetKey(behaviorID, parameterId);
		float value = tableData.getFloatField("value", -1.0f);

		m_Entries.insert(std::make_pair(hash, value));

		tableData.nextRow();
	}
	tableData.finalize();
}

float CDBehaviorParameterTable::GetValue(const uint32_t behaviorID, const std::string& name, const float defaultValue) {
	auto parameterID = this->m_ParametersList.find(name);
	if (parameterID == this->m_ParametersList.end()) return defaultValue;
	auto hash = GetKey(behaviorID, parameterID->second);

	// Search for specific parameter
	auto it = m_Entries.find(hash);
	return it != m_Entries.end() ? it->second : defaultValue;
}

std::map<std::string, float> CDBehaviorParameterTable::GetParametersByBehaviorID(uint32_t behaviorID) {
	uint64_t hashBase = behaviorID;
	std::map<std::string, float> returnInfo;
	for (auto& [parameterString, parameterId] : m_ParametersList) {
		uint64_t hash = GetKey(hashBase, parameterId);
		auto infoCandidate = m_Entries.find(hash);
		if (infoCandidate != m_Entries.end()) {
			returnInfo.insert(std::make_pair(parameterString, infoCandidate->second));
		}
	}
	return returnInfo;
}
