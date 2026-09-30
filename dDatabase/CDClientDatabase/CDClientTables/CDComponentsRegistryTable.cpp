#include "CDComponentsRegistryTable.h"
#include "CDFdb.h"
#include "Logger.h"
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

bool CDComponentsRegistryTable::LoadFromFdb() {
	m_FdbTable = nullptr;
	const auto* table = CDFdb::GetTable("ComponentsRegistry");
	if (!table) return false;

	m_TypeColumn = table->GetColumnIndex("component_type");
	m_ComponentIdColumn = table->GetColumnIndex("component_id");
	if (m_TypeColumn < 0 || m_ComponentIdColumn < 0) return false;

	const auto changed = CDFdb::FindChangedKeys(*table);
	if (!changed) return false;

	// Ids whose rows CDServer.sqlite changes are read from it once and kept; everything else comes from the fdb
	for (const auto id : *changed) LoadFromSqlite(static_cast<uint32_t>(id));
	LOG("ComponentsRegistry: reading from the fdb, %zu ids differ in CDServer.sqlite and are kept in memory", changed->size());

	m_FdbTable = table;
	return true;
}

void CDComponentsRegistryTable::LoadFromSqlite(uint32_t id) {
	auto& entries = GetEntriesMutable();

	// Get all components of this entity so we dont do a query for each component
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
}

int32_t CDComponentsRegistryTable::GetByIDAndType(uint32_t id, eReplicaComponentType componentType, int32_t defaultValue) {
	auto& entries = GetEntriesMutable();
	auto exists = entries.find(id);
	if (exists != entries.end()) {
		auto iter = entries.find(static_cast<uint64_t>(componentType) << 32 | static_cast<uint64_t>(id));
		return iter == entries.end() ? defaultValue : iter->second;
	}

	if (m_FdbTable) {
		// The last matching row wins, as when the rows are loaded into the map in file order
		int32_t result = defaultValue;
		const auto type = static_cast<int32_t>(componentType);
		m_FdbTable->ForEachRowWithKey(static_cast<int32_t>(id), [&](const FdbReader::Row& row) {
			if (row.GetInt(static_cast<uint32_t>(m_TypeColumn), 0) == type) {
				result = row.GetInt(static_cast<uint32_t>(m_ComponentIdColumn), -1);
			}
		});
		return result;
	}

	LoadFromSqlite(id);

	auto iter = entries.find(static_cast<uint64_t>(componentType) << 32 | static_cast<uint64_t>(id));

	return iter == entries.end() ? defaultValue : iter->second;
}
