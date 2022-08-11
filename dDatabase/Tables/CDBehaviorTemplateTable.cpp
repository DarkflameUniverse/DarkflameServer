#include "CDBehaviorTemplateTable.h"

#include "CDProvider.h"

CDBehaviorTemplate EmptyBehaviorTemplate {0, 0, 0, 0};

CD_PROVIDER(BehaviorTemplateProvider, uint32_t, CDBehaviorTemplate);

//! Constructor
CDBehaviorTemplateTable::CDBehaviorTemplateTable(void) {
    NEW_CD_PROVIDER(BehaviorTemplateProvider, "BehaviorTemplate", [](CppSQLite3Query& query) {
        CDBehaviorTemplate entry;
        
        entry.behaviorID = query.getIntField(0, -1);
        entry.templateID = query.getIntField(1, -1);
        entry.effectID = query.getIntField(2, -1);
        entry.effectHandle = CDTable::SetString(query.getStringField(3, ""));
        
        return std::make_pair(entry.behaviorID, entry);
    }, [](int32_t size) {
        return 40 * 1000 * 1000;
    }, false);
}

//! Destructor
CDBehaviorTemplateTable::~CDBehaviorTemplateTable(void) { }

//! Returns the table's name
std::string CDBehaviorTemplateTable::GetName(void) const {
    return "BehaviorTemplate";
}

const CDBehaviorTemplate& CDBehaviorTemplateTable::GetByBehaviorID(uint32_t behaviorID) const {
    return BehaviorTemplateProvider->GetEntry(behaviorID, EmptyBehaviorTemplate);
}

void CDBehaviorTemplateTable::LoadHost() {
    BehaviorTemplateProvider->LoadHost();
}
