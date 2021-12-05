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
        entry.effectHandle = tableData.getStringField(3, "");
        
        this->entries.push_back(entry);
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
