#pragma once

#include "CDTable.h"

enum class eReplicaComponentType : uint32_t;

class CDComponentsRegistryTable : public CDTable<CDComponentsRegistryTable> {
public:
	CDComponentsRegistryTable();
	int32_t GetByIDAndType(uint32_t id, eReplicaComponentType componentType, int32_t defaultValue = 0);
private:
	void ReadRow(CppSQLite3Query& rowData);
private:
	std::unordered_map<uint64_t, uint32_t> mappedEntries;
};
