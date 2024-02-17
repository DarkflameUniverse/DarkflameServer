#include "CDBehaviorTemplateTable.h"

namespace {
	std::unordered_set<std::string> m_EffectHandles;
};

void CDBehaviorTemplateTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM BehaviorTemplate");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM BehaviorTemplate");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		CDBehaviorTemplate entry;
		entry.behaviorID = tableData.getIntField("behaviorID", -1);
		entry.templateID = tableData.getIntField("templateID", -1);
		entry.effectID = tableData.getIntField("effectID", -1);
		auto candidateToAdd = tableData.getStringField(3, "");
		auto parameter = m_EffectHandles.find(candidateToAdd);
		if (parameter != m_EffectHandles.end()) {
			entry.effectHandle = parameter;
		} else {
			entry.effectHandle = m_EffectHandles.insert(candidateToAdd).first;
		}

		entries.insert(std::make_pair(entry.behaviorID, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

const CDBehaviorTemplate CDBehaviorTemplateTable::GetByBehaviorID(uint32_t behaviorID) {
	auto& entries = GetEntriesMutable();
	auto entry = entries.find(behaviorID);
	if (entry == entries.end()) {
		CDBehaviorTemplate entryToReturn;
		entryToReturn.behaviorID = 0;
		entryToReturn.effectHandle = m_EffectHandles.end();
		entryToReturn.effectID = 0;
		return entryToReturn;
	} else {
		return entry->second;
	}
}
