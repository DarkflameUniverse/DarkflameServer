#ifndef __ENTITYSYSTEM_H__
#define __ENTITYSYSTEM_H__

#include <cstdint>

#include "Archetype.h"

// Anonymous namespace contains "private" functions and variables
namespace {
	struct Record {
		ArchetypeBase* archetype;
		size_t index;
	};

	std::unordered_map<LWOOBJID, Record> entityIndex;

	using ArchetypeId = uint32_t;
	using ArchetypeSet = std::unordered_set<ArchetypeId>;
	using ComponentTypeId = std::type_index;
	std::unordered_map<ComponentTypeId, ArchetypeSet> componentTypeIndex;
}

// EntitySystem namespace contains "public" functions and variables
namespace EntitySystem {
	template <ComponentType CType>
	bool HasComponent(const LWOOBJID entityId) {
		ArchetypeBase* archetype = entityIndex[entityId].archetype;
		ArchetypeSet& archetypeSet = componentTypeIndex[std::type_index(typeid(CType))];
		return archetypeSet.count(archetype->id) != 0;
	}

	template <ComponentType CType>
	CType* GetComponent(const LWOOBJID entityId) {
		if (!HasComponent<CType>(entityId)) return nullptr;

		auto index = entityIndex[entityId].index;
		ArchetypeBase* archetype = entityIndex[entityId].archetype;

		return nullptr;
	}
}

#endif // !__ENTITYSYSTEM_H_
