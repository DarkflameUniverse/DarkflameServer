#pragma once

// Custom Classes
#include "CDTable.h"

enum class eReplicaComponentType : uint32_t;
struct CDComponentsRegistry {
	unsigned int id;                    //!< The LOT is used as the ID
	eReplicaComponentType component_type;        //!< See ComponentTypes enum for values
	unsigned int component_id;          //!< The ID used within the component's table (0 may either mean it's non-networked, or that the ID is actually 0
};


class CDComponentsRegistryTable : public CDTable {
private:
	std::map<uint64_t, uint32_t> mappedEntries; //id, component_type, component_id

public:
	CDComponentsRegistryTable();

	static const std::string GetTableName() { return "ComponentsRegistry"; };

	int32_t GetByIDAndType(uint32_t id, eReplicaComponentType componentType, int32_t defaultValue = 0);
};
