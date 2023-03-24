#include "CDComponentsRegistryTable.h"
#include "eReplicaComponentType.h"

#define CDCLIENT_CACHE_ALL

CDComponentsRegistryTable::CDComponentsRegistryTable(void) {

#ifdef CDCLIENT_CACHE_ALL
	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ComponentsRegistry");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	//this->entries.reserve(size);

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
#endif
}

int32_t CDComponentsRegistryTable::GetByIDAndType(uint32_t id, eReplicaComponentType componentType, int32_t defaultValue) {
	const auto& iter = this->mappedEntries.find(((uint64_t)componentType) << 32 | ((uint64_t)id));

	if (iter == this->mappedEntries.end()) {
		return defaultValue;
	}

	return iter->second;

#ifndef CDCLIENT_CACHE_ALL
	// Now get the data
	std::stringstream query;

	query << "SELECT * FROM ComponentsRegistry WHERE id = " << std::to_string(id);

	auto tableData = CDClientDatabase::ExecuteQuery(query.str());
	while (!tableData.eof()) {
		CDComponentsRegistry entry;
		entry.id = tableData.getIntField("id", -1);
		entry.component_type = tableData.getIntField("component_type", -1);
		entry.component_id = tableData.getIntField("component_id", -1);

		//this->entries.push_back(entry);

		//Darwin's stuff:
		const auto& it = this->mappedEntries.find(entry.id);
		if (it != mappedEntries.end()) {
			const auto& iter = it->second.find(entry.component_type);
			if (iter == it->second.end()) {
				it->second.insert(std::make_pair(entry.component_type, entry.component_id));
			}
		} else {
			std::map<unsigned int, unsigned int> map;
			map.insert(std::make_pair(entry.component_type, entry.component_id));
			this->mappedEntries.insert(std::make_pair(entry.id, map));
		}

		tableData.nextRow();
	}

	tableData.finalize();

	const auto& it2 = this->mappedEntries.find(id);
	if (it2 != mappedEntries.end()) {
		const auto& iter = it2->second.find(componentType);
		if (iter != it2->second.end()) {
			return iter->second;
		}
	}

	return defaultValue;
#endif
}

