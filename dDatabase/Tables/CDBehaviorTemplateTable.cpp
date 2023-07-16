#include "CDBehaviorTemplateTable.h"

namespace {
	std::vector<CDBehaviorTemplate> entries;
	std::unordered_map<uint32_t, CDBehaviorTemplate> entriesMappedByBehaviorID;
	std::unordered_set<std::string> m_EffectHandles;
};

void CDBehaviorTemplateTable::LoadTableIntoMemory() {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM BehaviorTemplate");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM BehaviorTemplate");
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

		entries.push_back(entry);
		entriesMappedByBehaviorID.insert(std::make_pair(entry.behaviorID, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDBehaviorTemplate> CDBehaviorTemplateTable::Query(std::function<bool(CDBehaviorTemplate)> predicate) {

	std::vector<CDBehaviorTemplate> data = cpplinq::from(entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const CDBehaviorTemplate CDBehaviorTemplateTable::GetByBehaviorID(uint32_t behaviorID) {
	auto entry = entriesMappedByBehaviorID.find(behaviorID);
	if (entry == entriesMappedByBehaviorID.end()) {
		CDBehaviorTemplate entryToReturn;
		entryToReturn.behaviorID = 0;
		entryToReturn.effectHandle = m_EffectHandles.end();
		entryToReturn.effectID = 0;
		return entryToReturn;
	} else {
		return entry->second;
	}
}

