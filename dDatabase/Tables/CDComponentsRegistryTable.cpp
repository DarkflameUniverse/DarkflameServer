#include "CDComponentsRegistryTable.h"
#include "eReplicaComponentType.h"
#include "dLogger.h"
#include "Game.h"

uint64_t CalculateId(uint64_t lhs, uint64_t rhs) {
	return (lhs << 32) | rhs;
}

void CDComponentsRegistryTable::ReadRow(CppSQLite3Query& rowData) {
	uint32_t id = rowData.getIntField("id", -1);
	eReplicaComponentType component_type = static_cast<eReplicaComponentType>(rowData.getIntField("component_type", 0));
	uint32_t component_id = rowData.getIntField("component_id", -1);

	auto insertedEntry = this->mappedEntries.insert_or_assign(CalculateId(id, static_cast<uint64_t>(component_type)), component_id);
	DluAssert(insertedEntry.second == true);
}

CDComponentsRegistryTable::CDComponentsRegistryTable() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ComponentsRegistry");
	while (!tableData.eof()) {
		ReadRow(tableData);
		tableData.nextRow();
	}
	tableData.finalize();
}

int32_t CDComponentsRegistryTable::GetByIDAndType(uint32_t id, eReplicaComponentType componentType, int32_t defaultValue) {
	const auto iter = this->mappedEntries.find(CalculateId(id, static_cast<uint64_t>(componentType)));
	return iter != this->mappedEntries.end() ? iter->second : defaultValue;
}
