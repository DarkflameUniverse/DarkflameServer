#pragma once

// Custom Classes
#include "CDTable.h"

#include <unordered_map>

enum class eReplicaComponentType : uint32_t;
struct CDComponentsRegistry {
	uint32_t id;                    //!< The LOT is used as the ID
	eReplicaComponentType component_type;        //!< See ComponentTypes enum for values
	uint32_t component_id;          //!< The ID used within the component's table (0 may either mean it's non-networked, or that the ID is actually 0
};


class CDComponentsRegistryTable : public CDTable<CDComponentsRegistryTable> {
private:
	std::unordered_map<uint64_t, uint32_t> mappedEntries; //id, component_type, component_id

public:
	void LoadValuesFromDatabase();
	int32_t GetByIDAndType(uint32_t id, eReplicaComponentType componentType, int32_t defaultValue = 0);
};
