#ifndef __ENTITYSYSTEM_H__
#define __ENTITYSYSTEM_H__

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include <utility>
#include <variant>

#include "Archetype.h"
#include "CharacterComponent.h" // TEMP
#include "DestroyableComponent.h" // TEMP
#include "SimplePhysicsComponent.h" // TEMP
#include "dCommonVars.h"
#include "ObjectIDManager.h"

/**
 * Archetype visitor structs (for use with std::visit)
*/
namespace {
	/**
	 * ComponentVisitor struct: Used to perform GetComponent function calls on an archetype using std::visit
	*/
	template <ComponentType CType>
	struct ComponentVisitor {
		const size_t index;

		explicit constexpr ComponentVisitor(const size_t index) noexcept : index{ index } {}

		/**
		 * The operator() for the ComponentVisitor struct. Used to get components from an archetype
		 * @returns A const pointer to the component if it is present in the archetype, nullptr if it is not
		*/
		constexpr CType* const operator()(auto&& archetype) { // TODO: There might be a way to use this to do compile-time checking...
			if constexpr (archetype->template HasComponent<CType>()) {
				return &archetype->template GetComponent<CType>(index);
			} else {
				return nullptr;
			}
		}
	};
}

/**
 * TODO: Class documentation
*/
class EntitySystem final {
public:
	/**
	 * Aliases
	*/
	using ArchetypeId = uint32_t;
	using ArchetypeSet = std::unordered_set<ArchetypeId>;
	using ArchetypeVariantPtr = std::variant<
		Archetype<CharacterComponent>*,
		Archetype<DestroyableComponent>*,
		Archetype<SimplePhysicsComponent>*,

		Archetype<CharacterComponent, DestroyableComponent>*,
		Archetype<DestroyableComponent, SimplePhysicsComponent>*,

		Archetype<CharacterComponent, DestroyableComponent, SimplePhysicsComponent>*
	>; // TODO: Figure out how to generate this automatically
	using ComponentTypeId = std::type_index;

	/**
	 * Adds entity to the entity system
	 * @param explicitId Explicit object ID to provide to entity
	 * @param componentArgs Arguments to be forwarded to component constructors
	*/
	template <ComponentType... CTypes>
	void CreateEntity(const LWOOBJID explicitId, CTypes&&... componentArgs) {
		Archetype<CTypes...>& archetype = GetArchetype<CTypes...>();

		// Need to handle entities with baked-in IDs in their info... but later
		// In fact, need to review ALL the id code for later
		// For now, just focusing on the archetype logic

		const size_t insertedIndex = archetype.size();
		archetype.CreateComponents(std::forward<CTypes>(componentArgs)...); // Create the components in the archetype
		m_EntityIndex.try_emplace(explicitId, ArchetypeRecord{ &archetype, insertedIndex }); // Create the corresponding pointers in the entity index
	}

	/**
	 * Overload for non-specified object ID
	 * @param componentArgs Arguments to be forwarded to component constructors
	*/
	template <ComponentType... CTypes>
	void CreateEntity(CTypes&&... componentArgs) {
		CreateEntity(ObjectIDManager::GenerateObjectID(), std::forward<CTypes>(componentArgs)...);
	}

	/**
	 * Mark an entity for deletion given a corresponding object ID TODO: IMPLEMENT
	 * @param entityId ID of the entity to mark for deletion
	*/
	void MarkForDeletion(const LWOOBJID entityId) {
		//const auto& itToRemove = m_EntityIndex.find(entityId); //perform the search in m_EntityIndex only once
		//m_EntitiesToDelete.emplace(entityId, std::move(itToRemove->second));
		//m_EntityIndex.erase(itToRemove);

		m_EntitiesToDelete.emplace(entityId);
	}

	/**
	 * Determine if an entity is associated with an Object ID
	 *
	*/
	[[nodiscard]] bool EntityExists(const LWOOBJID entityId) noexcept {
		return m_EntityIndex.count(entityId) != 0;
	}

	/**
	 * Determine if an entity has a component
	 * @param entityId Object ID of the entity to check
	 * @returns Boolean value representing whether component is present
	*/
	template <ComponentType CType>
	[[nodiscard]] bool HasComponent(const LWOOBJID entityId) {
		const auto& archetypeRecord = m_EntityIndex[entityId];
		const auto& hasComponentVisitor = [](auto&& archetype) { return archetype->template HasComponent<CType>(); };

		return std::visit(hasComponentVisitor, archetypeRecord.archetypePtr); // Using visitor pattern
	}

	/**
	 * Get a pointer to an entity component
	 * @param entityId Object ID of the entity to check
	 * @returns The pointer if the component exists, or nullptr if it does not
	*/
	template <ComponentType CType>
	[[nodiscard]] CType* const GetComponent(const LWOOBJID entityId) {
		const auto& archetypeRecord = m_EntityIndex[entityId];

		return std::visit(ComponentVisitor<CType>{ archetypeRecord.index }, archetypeRecord.archetypePtr); // Using visitor pattern
	}

protected:
	/**
	 * Method to create an entity archetype (relies on copy elision) TODO: Change?
	 * @param archetypeId The ID to assign to the created archetype
	*/
	template <ComponentType... CTypes>
	[[nodiscard]] Archetype<CTypes...> CreateArchetype(const ArchetypeId archetypeId) { // TODO: Noexcept?
		(m_ComponentTypeIndex[std::type_index(typeid(CTypes))].insert(archetypeId), ...); // Add the matching types to the component type index
		return Archetype<CTypes...>{ archetypeId }; // Return the created archetype
	}

	/**
	 * Method to get a reference to an entity archetype given the components it contains
	*/
	template <ComponentType... CTypes>
	[[nodiscard]] Archetype<CTypes...>& GetArchetype() {
		static auto archetype = CreateArchetype<CTypes...>(++m_CurrentArchetypeId);
		return archetype;
	}

private:
	friend class ArchetypeTest;

	ArchetypeId m_CurrentArchetypeId{ 0 };

	struct ArchetypeRecord {
		ArchetypeVariantPtr archetypePtr;
		size_t index;
	};

	std::unordered_map<LWOOBJID, ArchetypeRecord> m_EntityIndex;

	std::unordered_set<LWOOBJID> m_EntitiesToDelete;

	std::unordered_map<ComponentTypeId, ArchetypeSet> m_ComponentTypeIndex;
};

#endif // !__ENTITYSYSTEM_H
