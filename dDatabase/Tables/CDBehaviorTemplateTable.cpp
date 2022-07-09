#include "CDBehaviorTemplateTable.h"

//! Constructor
CDBehaviorTemplateTable::CDBehaviorTemplateTable(void) {
    
    // First, get the size of the table
    unsigned int size = 0;
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
        entry.behaviorID = tableData.getIntField(0, -1);
        entry.templateID = tableData.getIntField(1, -1);
        entry.effectID = tableData.getIntField(2, -1);
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

//! Destructor
CDBehaviorTemplateTable::~CDBehaviorTemplateTable(void) { }

//! Returns the table's name
std::string CDBehaviorTemplateTable::GetName(void) const {
    return "BehaviorTemplate";
}

//! Queries the table with a custom "where" clause
std::vector<CDBehaviorTemplate> CDBehaviorTemplateTable::Query(std::function<bool(CDBehaviorTemplate)> predicate) {
    
    std::vector<CDBehaviorTemplate> data = cpplinq::from(this->entries)
    >> cpplinq::where(predicate)
    >> cpplinq::to_vector();
    
    return data;
}

//! Gets all the entries in the table
std::vector<CDBehaviorTemplate> CDBehaviorTemplateTable::GetEntries(void) const {
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
