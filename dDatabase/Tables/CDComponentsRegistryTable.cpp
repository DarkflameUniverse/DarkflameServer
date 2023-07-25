#include "CDComponentsRegistryTable.h"
#include "eReplicaComponentType.h"

void CDComponentsRegistryTable::LoadValuesFromDatabase() {
	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ComponentsRegistry");
	while (!tableData.eof()) {
		CDComponentsRegistry entry;
		entry.id = tableData.getIntField("id", -1);
		entry.component_type = static_cast<eReplicaComponentType>(tableData.getIntField("component_type", 0));
		entry.component_id = tableData.getIntField("component_id", -1);

		this->mappedEntries.insert_or_assign(((uint64_t)entry.component_type) << 32 | ((uint64_t)entry.id), entry.component_id);

		tableData.nextRow();
	}

	tableData.finalize();
}

int32_t CDComponentsRegistryTable::GetByIDAndType(uint32_t id, eReplicaComponentType componentType, int32_t defaultValue) {
	auto iter = mappedEntries.find(((uint64_t)componentType) << 32 | ((uint64_t)id));

	if (iter == this->mappedEntries.end()) {
		return defaultValue;
	}

	return iter->second;
	// Now get the data
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM ComponentsRegistry WHERE id = ? AND component_type = ?;");
	query.bind(1, static_cast<int32_t>(id));
	query.bind(2, static_cast<int32_t>(componentType));

	auto tableData = query.execQuery();
	while (!tableData.eof()) {
		CDComponentsRegistry entry;
		entry.id = tableData.getIntField("id", -1);
		entry.component_type = static_cast<eReplicaComponentType>(tableData.getIntField("component_type", 0));
		entry.component_id = tableData.getIntField("component_id", -1);

		this->mappedEntries.insert_or_assign(((uint64_t)entry.component_type) << 32 | ((uint64_t)entry.id), entry.component_id);

		tableData.nextRow();
	}

	iter = this->mappedEntries.find(((uint64_t)componentType) << 32 | ((uint64_t)id));

	return iter == this->mappedEntries.end() ? defaultValue : iter->second;
}

