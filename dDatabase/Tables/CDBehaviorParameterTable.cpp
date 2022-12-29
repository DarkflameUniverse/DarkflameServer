#include "CDBehaviorParameterTable.h"
#include "GeneralUtils.h"

//! Constructor
CDBehaviorParameterTable::CDBehaviorParameterTable(void) {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM BehaviorParameter");
	uint32_t uniqueParameterId = 0;
	uint64_t hash = 0;
	while (!tableData.eof()) {
		CDBehaviorParameter entry;
		entry.behaviorID = tableData.getIntField(0, -1);
		auto candidateStringToAdd = std::string(tableData.getStringField(1, ""));
		auto parameter = m_ParametersList.find(candidateStringToAdd);
		if (parameter != m_ParametersList.end()) {
			entry.parameterID = parameter;
		} else {
			entry.parameterID = m_ParametersList.insert(std::make_pair(candidateStringToAdd, uniqueParameterId)).first;
			uniqueParameterId++;
		}
		hash = entry.behaviorID;
		hash = (hash << 31U) | entry.parameterID->second;
		entry.value = tableData.getFloatField(2, -1.0f);

		m_Entries.insert(std::make_pair(hash, entry));

		tableData.nextRow();
	}
	tableData.finalize();
	Game::logger->Log("CDBehaviorParameterTable", "tables size %i %i", m_Entries.size(), m_ParametersList.size());
}

//! Destructor
CDBehaviorParameterTable::~CDBehaviorParameterTable(void) {}

//! Returns the table's name
std::string CDBehaviorParameterTable::GetName(void) const {
	return "BehaviorParameter";
}

float CDBehaviorParameterTable::GetValue(const uint32_t behaviorID, const std::string& name, const float defaultValue) {
	auto parameterID = this->m_ParametersList.find(name);
	if (parameterID == this->m_ParametersList.end()) return defaultValue;

	uint64_t hash = behaviorID;

	hash = (hash << 31U) | parameterID->second;

	// Search for specific parameter
	const auto& it = m_Entries.find(hash);
	return it != m_Entries.end() ? it->second.value : defaultValue;
}

std::map<std::string, float> CDBehaviorParameterTable::GetParametersByBehaviorID(uint32_t behaviorID) {
	uint64_t hashBase = behaviorID;
	std::map<std::string, float> returnInfo;
	uint64_t hash;
	for (auto& parameterCandidate : m_ParametersList) {
		hash = (hashBase << 31U) | parameterCandidate.second;
		auto infoCandidate = m_Entries.find(hash);
		if (infoCandidate != m_Entries.end()) {
			returnInfo.insert(std::make_pair(infoCandidate->second.parameterID->first, infoCandidate->second.value));
		}
	}
	return returnInfo;
}
