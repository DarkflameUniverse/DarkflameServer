#include "CDBehaviorTemplateTable.h"

void CDBehaviorTemplateTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM BehaviorTemplate");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

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

		this->entries.push_back(entry);
		this->entriesMappedByBehaviorID.insert(std::make_pair(entry.behaviorID, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDBehaviorTemplate> CDBehaviorTemplateTable::Query(std::function<bool(CDBehaviorTemplate)> predicate) {

	std::vector<CDBehaviorTemplate> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const std::vector<CDBehaviorTemplate>& CDBehaviorTemplateTable::GetEntries() const {
	return this->entries;
}

const CDBehaviorTemplate CDBehaviorTemplateTable::GetByBehaviorID(uint32_t behaviorID) {
	auto entry = this->entriesMappedByBehaviorID.find(behaviorID);
	if (entry == this->entriesMappedByBehaviorID.end()) {
		CDBehaviorTemplate entryToReturn;
		entryToReturn.behaviorID = 0;
		entryToReturn.effectHandle = m_EffectHandles.end();
		entryToReturn.effectID = 0;
		return entryToReturn;
	} else {
		return entry->second;
	}
}
