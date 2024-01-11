#pragma once

#include <cstdint>

#include "Component.h"
#include "eReplicaComponentType.h"
#include "DestroyableComponent.h"
#include "SimplePhysicsComponent.h"

/*template <typename... Types>
struct ArchetypeEdge {
	Archetype<Types...>& add;
	Archetype<Types...>& remove;
};*/

// Require the template type to be of component base class
template <typename T>
concept ComponentType = std::is_base_of_v<Component, T>;

// Creat an alias for the container type
template <typename T>
using TypeContainer = std::vector<T>;

template <ComponentType... CTypes>
class Archetype final {
public:
	explicit Archetype(uint32_t id) noexcept { m_archetypeId = id; };
	~Archetype() = default;

	void CreateComponents(CTypes&&... args) {
		//m_Components.emplace_back(std::make_tuple(std::forward<CTypes>(args)...));
		(std::get<TypeContainer<CTypes>>(m_Components).emplace_back(std::forward<CTypes>(args)), ...);
	};

	template <std::integral IType>
	void DeleteComponents(IType index) {
		//m_Components[index] = std::move(m_Components.back());
		//m_Components.pop_back();
		((std::get<TypeContainer<CTypes>>(m_Components)[index] = std::move(std::get<TypeContainer<CTypes>>(m_Components).back())), ...); // Make this nicer to look at? :(
		(std::get<TypeContainer<CTypes>>(m_Components).pop_back(), ...);
	};

	template <ComponentType CType, std::integral IType>
	CType& GetComponent(IType index) {
		//return std::get<CType>(m_Components[index]);
		return std::get<TypeContainer<CType>>(m_Components)[index];
	};

	// Operator overloads (Should I even keep this?)
	/*template <std::integral IType>
	constexpr std::tuple<CTypes...>& operator[] (IType index) noexcept {
		return m_Components[index];
	}*/

private:
	uint32_t m_archetypeId;
	//std::vector<eReplicaComponentType> m_Types; // Maybe unneeded?
	//std::vector<std::tuple<CTypes...>> m_Components; // Maybe make it a tuple of vectors instead?
	std::tuple<TypeContainer<CTypes>...> m_Components; // Made it a tuple of vectors (may God help us all)
	//std::unordered_map<eReplicaComponentType, ArchetypeEdge<Types...>> edges;
};

// TODO: IMPLEMENT COMPILE-TIME TYPE ORDERING BY eReplicaType ENUM VALUE
// TODO: IMPLEMENT COMPONENT MOVING TO ANOTHER ARCHETYPE
