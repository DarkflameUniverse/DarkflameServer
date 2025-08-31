#include "CDComponentsRegistryTable.h"
#include "eReplicaComponentType.h"

void CDComponentsRegistryTable::LoadValuesFromDatabase() {
	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ComponentsRegistry");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		CDComponentsRegistry entry;
		entry.id = tableData.getIntField("id", -1);
		entry.component_type = static_cast<eReplicaComponentType>(tableData.getIntField("component_type", 0));
		entry.component_id = tableData.getIntField("component_id", -1);

		entries.insert_or_assign(static_cast<uint64_t>(entry.component_type) << 32 | static_cast<uint64_t>(entry.id), entry.component_id);
		entries.insert_or_assign(entry.id, 0);

		tableData.nextRow();
	}

	tableData.finalize();
}

int32_t CDComponentsRegistryTable::GetByIDAndType(uint32_t id, eReplicaComponentType componentType, int32_t defaultValue) {
	auto& entries = GetEntriesMutable();
	auto exists = entries.find(id);
	if (exists != entries.end()) {
		auto iter = entries.find(static_cast<uint64_t>(componentType) << 32 | static_cast<uint64_t>(id));
		return iter == entries.end() ? defaultValue : iter->second;
	}

	// Now get the data. Get all components of this entity so we dont do a query for each component
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM ComponentsRegistry WHERE id = ?;");
	query.bind(1, static_cast<int32_t>(id));

	auto tableData = query.execQuery();

	while (!tableData.eof()) {
		CDComponentsRegistry entry;
		entry.id = tableData.getIntField("id", -1);
		entry.component_type = static_cast<eReplicaComponentType>(tableData.getIntField("component_type", 0));
		entry.component_id = tableData.getIntField("component_id", -1);

		entries.insert_or_assign(static_cast<uint64_t>(entry.component_type) << 32 | static_cast<uint64_t>(entry.id), entry.component_id);

		tableData.nextRow();
	}

	entries.insert_or_assign(id, 0);

	auto iter = entries.find(static_cast<uint64_t>(componentType) << 32 | static_cast<uint64_t>(id));

	return iter == entries.end() ? defaultValue : iter->second;
}
