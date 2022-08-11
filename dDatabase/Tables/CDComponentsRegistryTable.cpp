
#include "CDComponentsRegistryTable.h"

#define CDCLIENT_CACHE_ALL

#include "CDProvider.h"

CD_PROVIDER(ComponentsRegistryProvider, size_t, int32_t);

//! Constructor
CDComponentsRegistryTable::CDComponentsRegistryTable(void) {
	NEW_CD_PROVIDER(ComponentsRegistryProvider, "ComponentsRegistry", [](CppSQLite3Query& query) {
		size_t hash = 0;
		
		int32_t componentID = query.getIntField(0, -1);
		int32_t componentType = query.getIntField(1, -1);
		
		hash = componentID;
		hash = hash << 32;
		hash |= componentType;
		
		int32_t componentHandle = query.getIntField(2, -1);
		
		return std::make_pair(hash, componentHandle);
	}, [](int32_t size) {
		return 10 * 1000 * 1000;
	}, true);
}

//! Destructor
CDComponentsRegistryTable::~CDComponentsRegistryTable(void) { }

//! Returns the table's name
std::string CDComponentsRegistryTable::GetName(void) const {
    return "ComponentsRegistry";
}

int32_t CDComponentsRegistryTable::GetByIDAndType(uint32_t id, uint32_t componentType, int32_t defaultValue) {
	size_t hash = 0;
	hash = id;
	hash = hash << 32;
	hash |= componentType;
	
	return ComponentsRegistryProvider->GetEntry(hash, defaultValue);
}

void CDComponentsRegistryTable::LoadHost() {
	ComponentsRegistryProvider->LoadHost();
}
