#ifndef __ARCHETYPE_H__
#define __ARCHETYPE_H__

#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Component.h"

// Require the template type to be of component base class
template <typename T>
concept ComponentType = std::is_base_of_v<Component, T>;

/**
 * Base class to allow pointer/reference resolution
 * Contains less-performant versions of methods to allow runtime resolution of component types
*/
class IArchetype {
public:
	/**
	 * Create an alias type for the Archetype ID
	*/
	using ArchetypeId = uint32_t;
	ArchetypeId id{ 0 };

	/**
	 * Create an alias for the archetype component container type
	*/
	template <typename T>
	using ContainerType = std::vector<T>;

	/**
	 * Check if a component type is contained by the archetype
	 * @returns Boolean value representing whether the component type is present in the archetype
	*/
	template <ComponentType CType>
	[[nodiscard]] bool HasComponent() noexcept {
		return m_ContainerPointers.contains(std::type_index(typeid(CType))) != 0;
	}

	/**
	 * Get component container by way of indirect pointer/reference
	 * @returns A reference to the component container
	*/
	template <ComponentType CType>
	[[nodiscard]] ContainerType<CType>& Container() {
		return *reinterpret_cast<ContainerType<CType>*>(m_ContainerPointers[std::type_index(typeid(CType))]);
	}

	virtual ~IArchetype() = default;

protected:
	IArchetype(ArchetypeId id) noexcept : id{ id } {}

	/**
	 * Unordered map containing void pointers to each container
	*/
	std::unordered_map<std::type_index, void*> m_ContainerPointers;
};

/**
 * The archetype class stores a variable number of entity component types TODO: EXPAND ON
*/
template <ComponentType... CTypes>
class Archetype final : public IArchetype {
public:
	constexpr explicit Archetype(ArchetypeId id) noexcept : IArchetype{ id } {
		// Reserve 16 KB of memory for the sum of all vectors ahead of time
		constexpr size_t compBytes = (sizeof(CTypes) + ...);
		constexpr size_t reservedBytes = 16000;
		constexpr size_t reserveNumEntries = reservedBytes / compBytes;
		(Container<CTypes>().reserve(reserveNumEntries), ...);

		// Create void pointers to each component container present
		(m_ContainerPointers.emplace(typeid(CTypes), reinterpret_cast<void*>(&Container<CTypes>())), ...);
	}

	/**
	 * Constructors and assignment operators
	*/
	Archetype(Archetype& other) noexcept : IArchetype{ std::copy(other) } { /*UpdatePointers();*/ } // Copy constructor
	Archetype(Archetype&& other) noexcept : IArchetype{ std::move(other) } { /*UpdatePointers();*/ } // Move constructor
	Archetype& operator=(Archetype& other) noexcept { // Copy assignment operator
		IArchetype::operator=(std::copy(other));
		/*UpdatePointers();*/
	}
	Archetype& operator=(Archetype&& other) noexcept { // Move assignment operator
		IArchetype::operator=(std::move(other));
		/*UpdatePointers();*/
	}

	/**
	 * Get the size of the archetype (by taking the size of the first member container)
	 * @returns The size of the archetype's containers
	*/
	[[nodiscard]] constexpr size_t size() noexcept {
		return std::get<0>(m_Components).size();
	}

	/**
	 * Get a reference to the component container of an archetype.
	 * @returns A reference to the archetype's container of components
	*/
	template <ComponentType CType>
	[[nodiscard]] constexpr ContainerType<CType>& Container() noexcept {
		static_assert(hasComponent<CType>, "Archetype does not have container of requested component!"); // Compile-time verification
		return std::get<ContainerType<CType>>(m_Components);
	}

	/**
	 * Creates the archetype's components at the end of the container.
	 * @param componentArgs Arguments to be forwarded to the component constructors
	*/
	void CreateComponents(CTypes&&... componentArgs) noexcept {
		(Container<CTypes>().emplace_back(std::forward<CTypes>(componentArgs)), ...);
	}

	/**
	 * Delete's the archetype's components at a specified container index, then moves the last element in the container to it.
	 * @param index The archetype container index to delete
	*/
	void DeleteComponents(const size_t index) {
		((Container<CTypes>()[index] = std::move(Container<CTypes>().back()),
			Container<CTypes>().pop_back()), ...);
	}

	/**
	 * Gets a single archetype component at a specified container index.
	 * @param index The archetype container index to get
	 * @returns A reference to the component type specified as a template argument
	*/
	template <ComponentType CType>
	[[nodiscard]] CType& GetComponent(const size_t index) {
		return Container<CType>()[index];
	}

	/**
	 * Static function-like boolean that "returns" if an archetype contains a specified component
	*/
	template <ComponentType CType>
	static constexpr bool hasComponent = std::disjunction_v<std::is_same<CType, CTypes>...>;

	/**
	 * Contains the number of component types an archetype consists of
	*/
	static constexpr size_t numTypes = sizeof...(CTypes);

	template<ComponentType CType>
	struct contains : std::disjunction<std::is_same<CType, CTypes>...> {};

private:
	/**
	 * Update void pointer locations in memory (to be called by move and copy constructors)
	*/
	constexpr void UpdatePointers() noexcept {
		((m_ContainerPointers[std::type_index(typeid(CTypes))] = reinterpret_cast<void*>(Container<CTypes>())), ...);
	}

	std::tuple<ContainerType<CTypes>...> m_Components; // Made it a tuple of vectors (may God help us all)
	//std::unordered_map<eReplicaComponentType, ArchetypeEdge<Types...>> edges;
};

#endif // !__ARCHETYPE_H__

// TODO: IMPLEMENT COMPILE-TIME TYPE ORDERING BY eReplicaType ENUM VALUE
// TODO: SEE WHICH FUNCTIONS CAN BE SAFELY MADE NOEXCEPT
// TODO: CREATE CUSTOM ITERATOR
