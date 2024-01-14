#ifndef __ARCHETYPE_H__
#define __ARCHETYPE_H__

#include <cstdint>
#include "Component.h"

// Create an alias for the archetype component container type
template <typename T>
using ComponentContainerType = std::vector<T>;

// Require the template type to be of component base class
template <typename T>
concept ComponentType = std::is_base_of_v<Component, T>;

// Require the index type to be integral
template <typename T>
concept IntegralType = std::is_integral_v<T>;

/**
 * The archetype class stores a variable number of entity component types TODO: EXPAND ON
*/
template <ComponentType... CTypes>
class Archetype final {
public:
	explicit Archetype(uint32_t id) noexcept : m_archetypeId{ id } {
		// Reserve 16 KB of memory for the sum of all vectors ahead of time
		constexpr size_t compBytes = (sizeof(CTypes) + ...);
		constexpr size_t reservedBytes = 16000;
		constexpr size_t reserveNumEntries = reservedBytes / compBytes;
		(ComponentContainer<CTypes>().reserve(reserveNumEntries), ...);
	}

	/**
	 * Get a reference to the component container of an archetype.
	 * @returns A reference to the archetype's container of components
	*/
	template <ComponentType CType>
	ComponentContainerType<CType>& ComponentContainer() {
		return std::get<ComponentContainerType<CType>>(m_Components);
	}

	/**
	 * Creates the archetype's components at the end of the container.
	 * @param componentArgs Arguments to be forwarded to the component constructors
	*/
	void CreateComponents(CTypes&&... componentArgs) {
		(ComponentContainer<CTypes>().emplace_back(std::forward<CTypes>(componentArgs)), ...);
	}

	/**
	 * Delete's the archetype's components at a specified container index, then moves the last element in the container to it.
	 * @param index The archetype container index to delete
	*/
	template <IntegralType IType>
	void DeleteComponents(IType index) {
		((ComponentContainer<CTypes>()[index] = std::move(ComponentContainer<CTypes>().back())), ...);
		(ComponentContainer<CTypes>().pop_back(), ...);
	}

	/**
	 * Gets a single archetype component at a specified container index.
	 * @param index The archetype container index to get
	 * @returns A reference to the component type specified as a template argument
	*/
	template <ComponentType CType, IntegralType IType>
	CType& GetComponent(IType index) {
		return ComponentContainer<CType>()[index];
	}

private:
	uint32_t m_archetypeId; // The ID of the archetype
	//std::vector<eReplicaComponentType> m_Types; // Maybe unneeded?
	std::tuple<ComponentContainerType<CTypes>...> m_Components; // Made it a tuple of vectors (may God help us all)
	//std::unordered_map<eReplicaComponentType, ArchetypeEdge<Types...>> edges;
};

#endif // !__ARCHETYPE_H__

// TODO: IMPLEMENT COMPILE-TIME TYPE ORDERING BY eReplicaType ENUM VALUE
// TODO: SEE WHICH FUNCTIONS CAN BE SAFELY MADE NOEXCEPT
// TODO: CREATE CUSTOM ITERATOR
