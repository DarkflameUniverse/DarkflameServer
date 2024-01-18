#ifndef __ARCHETYPE_H__
#define __ARCHETYPE_H__

#include <cstdint>
#include <type_traits>
#include "Component.h"

// Create an alias for the archetype ID type
using ArchetypeId = uint32_t;

// Create an alias for the archetype component container type
template <typename T>
using ComponentContainerType = std::vector<T>;

// Require the template type to be of component base class
template <typename T>
concept ComponentType = std::is_base_of_v<Component, T>;

// Base struct to allow pointer/reference resolution
struct ArchetypeBase {
	ArchetypeId id; // The ID of the archetype

	constexpr ArchetypeBase(ArchetypeId id) noexcept : id{ id } {}
	virtual ~ArchetypeBase() = default;
};

/**
 * The archetype class stores a variable number of entity component types TODO: EXPAND ON
*/
template <ComponentType... CTypes>
class Archetype final : public ArchetypeBase {
public:
	constexpr explicit Archetype(ArchetypeId id) noexcept : ArchetypeBase{ id } {
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
	constexpr ComponentContainerType<CType>& ComponentContainer() noexcept {
		static_assert(hasComponent<CType>, "Archetype does not have container of requested component!"); // Compile-time verification
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
	void DeleteComponents(const size_t index) {
		((ComponentContainer<CTypes>()[index] = std::move(ComponentContainer<CTypes>().back()),
		ComponentContainer<CTypes>().pop_back()), ...);
	}

	/**
	 * Gets a single archetype component at a specified container index.
	 * @param index The archetype container index to get
	 * @returns A reference to the component type specified as a template argument
	*/
	template <ComponentType CType>
	CType& GetComponent(const size_t index) {
		return ComponentContainer<CType>()[index];
	}

	/**
	 * Function-like static bool that "returns" if an archetype contains a specified component
	*/
	template <ComponentType CType>
	static constexpr bool hasComponent = std::disjunction_v<std::is_same<CType, CTypes>...>;

	/**
	 * Contains the number of component types an archetype consists of
	*/
	static constexpr size_t numTypes = sizeof...(CTypes);

private:
	std::tuple<ComponentContainerType<CTypes>...> m_Components; // Made it a tuple of vectors (may God help us all)
	//std::unordered_map<eReplicaComponentType, ArchetypeEdge<Types...>> edges;
};

// Static assertions for testing purposes


#endif // !__ARCHETYPE_H__

// TODO: IMPLEMENT COMPILE-TIME TYPE ORDERING BY eReplicaType ENUM VALUE
// TODO: SEE WHICH FUNCTIONS CAN BE SAFELY MADE NOEXCEPT
// TODO: CREATE CUSTOM ITERATOR
