#ifndef __ENTITYSYSTEM_H__
#define __ENTITYSYSTEM_H__

#include <cstdint>

#include "Archetype.h"

// Anonymous namespace contains "private" functions and variables
namespace {
	struct ArchetypeRecord {
		IArchetype* archetype; // Could we potentially make this std::variant in order to deduce the type?
		size_t index;
	};

	// Used to lookup components in archetypes
	//using ArchetypeMap = std::unordered_map<ArchetypeId, ArchetypeRecord>;
	//std::unordered_map<ArchetypeId, ArchetypeRecord> ArchetypeMap;
	//std::unordered_map<ComponentId, ArchetypeMap> component_index;

	std::unordered_map<LWOOBJID, ArchetypeRecord> entityIndex;

	using ArchetypeId = uint32_t;
	using ArchetypeSet = std::unordered_set<ArchetypeId>;
	using ComponentTypeId = std::type_index;
	std::unordered_map<ComponentTypeId, ArchetypeSet> componentTypeIndex;
}

// EntitySystem namespace contains "public" functions and variables
namespace EntitySystem {

	struct MetaType {
		Archetype<CharacterComponent, DestroyableComponent, SimplePhysicsComponent> a;

		Archetype<CharacterComponent, DestroyableComponent> aa;
		Archetype<DestroyableComponent, SimplePhysicsComponent> ab;

		Archetype<CharacterComponent> aaa;
		Archetype<DestroyableComponent> aab;
		Archetype<SimplePhysicsComponent> aac;
	};
	// Of those listed above, the only ones that could possibly contain a character component (for example) would be:

	struct mtt /*MetaTypeSlice<CharacterComponent>*/ {
		Archetype<CharacterComponent, DestroyableComponent, SimplePhysicsComponent> a;

		Archetype<CharacterComponent, DestroyableComponent> aa;

		Archetype<CharacterComponent> aaa;
	};
	// If we store the typenum of a container and associate it with the object ID, we can therefore find the valid archetype

	struct mtt2 /*MetaTypeSliceNum<CharacterComponent, 2>*/ {
		Archetype<CharacterComponent, DestroyableComponent> aa;
	};
	// We can then get the component container that corresponds to this, and the index number we also stored of the component

	/**
	 * There are two issues with this. While both the number of types an archetype contains and the types it stores are known at compile time (and thus our search terms
	 * should be able to be template-ized), the actual type of the given return is not known at compile time. We can probably generate some kind of reference map for these
	 * but storing references of different types will be challenging
	*/

	//NOTES: could std::integer_sequence be used in this?
	//NOTES: Could also make a funtion that takes typelists and returns their properties...

	// TEST AREA BELOW

	// GetSize
	template <typename ...Ts>
	constexpr size_t GetNumTypes() {
		return sizeof...(Ts);
	}

	struct EntityRecord {
		size_t archetypeTypeNum;
		//Archetype ID?
		// If we know the number of types stored AND the type being searched for, that would work, because each type should only recur once in an archetype
	};

	//std::unordered_map<ComponentTypeId, nTypes> componentTypeIndex

	/**
	 * Creates an archetype for a given typeset and provides singleton access to it
	*/
	template <ComponentType ...CTypes>
	Archetype<CTypes...>& GetArchetype() {
		static Archetype<CTypes...> archetype{ 0 };
		LOG("Got archetype!");
		return archetype;
	}

	// Recursively generate new archetypes TODO: FIX RECURSION
	// (Also try and ditch the singleton design pattern for a factory(?) approach)
	template <ComponentType CType, ComponentType ...CTypes>
	void CreateArchetypes() {
		GetArchetype<CType, CTypes...>(); // 100% needed
		if constexpr (sizeof...(CTypes)) {
			(GetArchetype<CTypes>(), ...);
			CreateArchetypes<CTypes...>();
		}
	}

	// Try and use our indexes here
	/*template <ComponentType CType, size_t numTypesStored>
	auto& FindArchetype() {
		// *MAGIC*
		return; //Return a reference to the archetype matching these criterion
	}*/

	template <ComponentType ...CTypes>
	void CreateComponents(const LWOOBJID entityId, CTypes&&... componentArgs) {
		auto& archetype = GetArchetype<CTypes...>(); // find the archetype matching the types inputted
		archetype.CreateComponents(std::forward<CTypes>(componentArgs)...);
	}

	template <ComponentType CType>
	bool HasComponent(const LWOOBJID entityId) {
		IArchetype* const archetype = entityIndex[entityId].archetype; // Gets a pointer to the archetype containing the entity ID
		ArchetypeSet& archetypeSet = componentTypeIndex[std::type_index(typeid(CType))]; // Gets the component container corresponding to the selected component type
		return archetypeSet.count(archetype->id) != 0; // Check that the component exists within there
	}

	/*template <ComponentType CType>
	bool HasComponent(const LWOOBJID entityId) {
		//auto& archetype = //GetArchetypeByEntityId(); //TODO - This CAN'T be known at compile time
		return GetArchetype<DestroyableComponent>().hasComponent<CType>; //return archetype::hasComponent<CType>;
	}*/

	template <ComponentType CType>
	CType* GetComponent(const LWOOBJID entityId) {
		if (!HasComponent<CType>(entityId)) return nullptr;

		auto& archetypeRecord = entityIndex[entityId];
		IArchetype* const archetype = archetypeRecord.archetype;
		return &archetype->Container<CType>()[archetypeRecord.index];
	}
}

#endif // !__ENTITYSYSTEM_H_


/*namespace ArchetypeSystem {
	// CONCEPTUALIZATION
	using ArchetypeId = uint32_t;

	using ArchetypeChar = Archetype<CharacterComponent>;
	using ArchetypeDest = Archetype<DestroyableComponent>;
	using ArchetypeSimp = Archetype<SimplePhysicsComponent>;

	using CombArchetypeCharDest = Archetype<CharacterComponent, DestroyableComponent>;
	using CombArchetypeDestSimp = Archetype<DestroyableComponent, SimplePhysicsComponent>;

	using CombArchetypeCharDestSimp = Archetype<CharacterComponent, DestroyableComponent, SimplePhysicsComponent>;

	// Think of the functions!

}*/
