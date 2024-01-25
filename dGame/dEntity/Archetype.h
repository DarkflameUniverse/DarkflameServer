#ifndef __ARCHETYPE_H__
#define __ARCHETYPE_H__

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Component.h"

// Require the template type to be of component base class
template <typename T>
concept ComponentType = std::derived_from<T, Component>;

// Forward declaration of archetype class
template <ComponentType... CTypes>
class Archetype;

// Container struct for storing component data
template <typename T>
struct Container {
	using type = T; // Alias to allow retrieval of entry type

	using storage_type = std::vector<T>; //Alias for the archetype component container type
	//using storage_type = std::array<T, 100>;

	storage_type entries;
};

/**
 * Base class to allow pointer/reference resolution
 * Contains less-performant versions of methods to allow runtime resolution of component types
*/
class ArchetypeBase {
public:
	/**
	 * Create an alias type for the Archetype ID
	*/
	using ArchetypeId = size_t;
	ArchetypeId id{ 0 };

	/**
	 * Check if a component type is contained by the archetype
	 * @returns Boolean value representing whether the component type is present in the archetype
	*/
	template <ComponentType CType>
	[[nodiscard]] bool HasComponent() noexcept {
		return dynamic_cast<Container<CType>&>(*this).template HasComponent<CType>(); // TODO: Change this method!
	}

	/**
	 * Get component container by way of indirect pointer/reference
	 * @returns A reference to the component container
	*/
	template <ComponentType CType>
	[[nodiscard]] Container<CType>::storage_type& GetContainer() { // TODO: Try to avoid using this!
		return dynamic_cast<Container<CType>&>(*this).template Container<CType>::entries;
	}

	virtual ~ArchetypeBase() = default;

protected:
	ArchetypeBase() = default;

	constexpr explicit ArchetypeBase(ArchetypeId id) noexcept : id{ id } {};
};

/**
 * The archetype class stores a variable number of entity component types TODO: EXPAND ON
*/
template <ComponentType... CTypes>
class Archetype final : public ArchetypeBase, public Container<CTypes>... {
public:
	/**
	 * Alias that extracts the type of the Nth element passed as a template argument
	*/
	template <size_t N>
	using type_index = std::tuple_element<N, std::tuple<CTypes...>>::type;

	/**
	 * Constructor
	*/
	constexpr explicit Archetype(ArchetypeId id) noexcept : ArchetypeBase{ id } {
		// Reserve 16 KB of memory for the sum of all vectors ahead of time
		constexpr size_t compBytes = (sizeof(CTypes) + ...);
		constexpr size_t reservedBytes = 16000;
		constexpr size_t reserveNumEntries = reservedBytes / compBytes;
		(Container<CTypes>::entries.reserve(reserveNumEntries), ...);
	}

	/**
	 * Get the size of the archetype (by taking the size of the first member container)
	 * @returns The size of the archetype's containers
	*/
	[[nodiscard]] constexpr size_t size() noexcept {
		Container<type_index<0>>::entries.size();
		return Container<type_index<0>>::entries.size();
	}

	/**
	 * Get if the container is empty (by only checking the first member container)
	 * @returns Boolean representing whether the container is empty
	*/
	[[nodiscard]] constexpr bool empty() noexcept {
		return Container<type_index<0>>::entries.empty();
	}

	/**
	 * Creates the archetype's components at the end of the container.
	 * @param componentArgs Arguments to be forwarded to the component constructors
	*/
	constexpr void CreateComponents(CTypes&&... componentArgs) noexcept {
		(Container<CTypes>::entries.emplace_back(std::forward<CTypes>(componentArgs)), ...);
	}

	/**
	 * Delete's the archetype's components at a specified container index, then moves the last element in the container to it.
	 * @param index The archetype container index to delete
	*/
	constexpr void DeleteComponents(const size_t index) {
		if (empty()) return; // Do not delete if the container is already empty

		((Container<CTypes>::entries.at(index) = std::move(Container<CTypes>::entries.back()),
			Container<CTypes>::entries.pop_back()), ...);
	}

	/**
	 * Gets a single archetype component at a specified container index.
	 * @param index The archetype container index to get
	 * @returns A reference to the component type specified as a template argument
	*/
	template <ComponentType CType>
	[[nodiscard]] CType& GetComponent(const size_t index) {
		return Container<CType>::entries.at(index);
	}

	/**
	 * Static function that returns if an archetype contains a specified component
	 * @returns Boolean representing component's presence
	*/
	template <ComponentType CType>
	[[nodiscard]] static constexpr bool HasComponent() noexcept {
		return std::disjunction_v<std::is_same<CType, CTypes>...>;
	}

	/**
	 * Contains the number of component types an archetype consists of
	*/
	static constexpr size_t num_types = sizeof...(CTypes);

	template<ComponentType CType>
	struct contains : std::disjunction<std::is_same<CType, CTypes>...> {};

	template<ComponentType CType>
	using contains_v = contains<CType>::value;

private:
	//std::unordered_map<eReplicaComponentType, ArchetypeEdge<Types...>> edges;
};

#endif // !__ARCHETYPE_H__

// TODO: IMPLEMENT COMPILE-TIME TYPE ORDERING BY eReplicaType ENUM VALUE
// TODO: SEE WHICH FUNCTIONS CAN BE SAFELY MADE NOEXCEPT
// TODO: CREATE CUSTOM ITERATOR
