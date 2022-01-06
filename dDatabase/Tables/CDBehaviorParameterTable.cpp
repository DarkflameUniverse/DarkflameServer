#include "CDBehaviorParameterTable.h"
#include "GeneralUtils.h"

//! Constructor
CDBehaviorParameterTable::CDBehaviorParameterTable(void) {
#ifdef CDCLIENT_CACHE_ALL
    auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM BehaviorParameter");
    while (!tableData.eof()) {
        CDBehaviorParameter entry;
        entry.behaviorID = tableData.getIntField(0, -1);
        entry.parameterID = tableData.getStringField(1, "");
        entry.value = tableData.getFloatField(2, -1.0f);

		//Check if we have an entry with this ID:
		auto it = m_entries.find(entry.behaviorID);
		if (it != m_entries.end()) {
			it->second.insert(std::make_pair(entry.parameterID, entry.value));
		}
		else { 
			//Otherwise, insert it:
			m_entries.insert(std::make_pair(entry.behaviorID, std::map<std::string, float>()));
			auto jit = m_entries.find(entry.behaviorID);

			//Add our value as well:
			jit->second.insert(std::make_pair(entry.parameterID, entry.value));
		}

        tableData.nextRow();
    }

	tableData.finalize();
#endif
}

//! Destructor
CDBehaviorParameterTable::~CDBehaviorParameterTable(void) { }

//! Returns the table's name
std::string CDBehaviorParameterTable::GetName(void) const {
    return "BehaviorParameter";
}

float CDBehaviorParameterTable::GetEntry(const uint32_t behaviorID, const std::string& name) 
{
	size_t hash = 0;
	GeneralUtils::hash_combine(hash, behaviorID);
	GeneralUtils::hash_combine(hash, name);

	// Search for specific perameter
	const auto& it = m_Entries.find(hash);
	if (it != m_Entries.end()) {
		return it->second;
	}

	// Check if this behavior has already been checked
	const auto& itChecked = m_Entries.find(behaviorID);
	if (itChecked != m_Entries.end()) {
		return itChecked->second;
	}

#ifndef CDCLIENT_CACHE_ALL
    auto tableData = CDClientDatabase::ExecuteQueryWithArgs(
        "SELECT parameterID, value FROM BehaviorParameter WHERE behaviorID = %u;",
        behaviorID);

	m_Entries.insert_or_assign(behaviorID, 0);
	
	while (!tableData.eof()) {
        const std::string parameterID = tableData.getStringField(0, "");
        const float value = tableData.getFloatField(1, 0);

		size_t parameterHash = 0;
		GeneralUtils::hash_combine(parameterHash, behaviorID);
		GeneralUtils::hash_combine(parameterHash, parameterID);

		m_Entries.insert_or_assign(parameterHash, value);

        tableData.nextRow();
    }

	const auto& it2 = m_Entries.find(hash);
	if (it2 != m_Entries.end()) {
		return it2->second;
	}
#endif

	return 0;
}
