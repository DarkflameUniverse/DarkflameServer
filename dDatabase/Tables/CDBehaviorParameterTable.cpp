#include "CDBehaviorParameterTable.h"
#include "GeneralUtils.h"

//! Constructor
CDBehaviorParameterTable::CDBehaviorParameterTable(void) {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM BehaviorParameter");
	size_t hash = 0;
	while (!tableData.eof()) {
		hash = 0;
		CDBehaviorParameter entry;
		entry.behaviorID = tableData.getIntField(0, -1);
		auto candidateStringToAdd = std::string(tableData.getStringField(1, ""));
		auto parameter = m_ParametersList.find(candidateStringToAdd);
		if (parameter != m_ParametersList.end()) {
			entry.parameterID = parameter;
		} else {
			entry.parameterID = m_ParametersList.insert(candidateStringToAdd).first;
		}
		entry.value = tableData.getFloatField(2, -1.0f);

		GeneralUtils::hash_combine(hash, entry.behaviorID);
		GeneralUtils::hash_combine(hash, *entry.parameterID);

		auto it = m_Entries.find(entry.behaviorID);
		m_ParametersList.insert(*entry.parameterID);
		m_Entries.insert(std::make_pair(hash, entry));

		tableData.nextRow();
	}
	tableData.finalize();
}

//! Destructor
CDBehaviorParameterTable::~CDBehaviorParameterTable(void) {}

//! Returns the table's name
std::string CDBehaviorParameterTable::GetName(void) const {
	return "BehaviorParameter";
}

CDBehaviorParameter CDBehaviorParameterTable::GetEntry(const uint32_t behaviorID, const std::string& name, const float defaultValue) {
	CDBehaviorParameter returnValue;
	returnValue.behaviorID = 0;
	returnValue.parameterID = m_ParametersList.end();
	returnValue.value = defaultValue;

	size_t hash = 0;
	GeneralUtils::hash_combine(hash, behaviorID);
	GeneralUtils::hash_combine(hash, name);

	// Search for specific parameter
	const auto& it = m_Entries.find(hash);
	return it != m_Entries.end() ? it->second : returnValue;
}

std::map<std::string, float> CDBehaviorParameterTable::GetParametersByBehaviorID(uint32_t behaviorID) {
	size_t hash;
	std::map<std::string, float> returnInfo;
	for (auto parameterCandidate : m_ParametersList) {
		hash = 0;
		GeneralUtils::hash_combine(hash, behaviorID);
		GeneralUtils::hash_combine(hash, parameterCandidate);
		auto infoCandidate = m_Entries.find(hash);
		if (infoCandidate != m_Entries.end()) {
			returnInfo.insert(std::make_pair(*(infoCandidate->second.parameterID), infoCandidate->second.value));
		}
	}
	return returnInfo;
}
