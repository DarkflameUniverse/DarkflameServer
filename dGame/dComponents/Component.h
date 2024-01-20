#pragma once

#include "tinyxml2.h"

class Entity;

/**
 * Component base class, provides methods for game loop updates, usage events and loading and saving to XML.
 */
class Component
{
public:
	Component(const LWOOBJID& parentEntityId) noexcept;
	virtual ~Component() = 0;

	/**
	 * Gets the owner of this component
	 * @return the owner of this component
	 */
	Entity* GetParent() const;

	/**
	 * Updates the component in the game loop
	 * @param deltaTime time passed since last update
	 */
	virtual void Update(float deltaTime);

	/**
	 * Event called when this component is being used, e.g. when some entity interacted with it
	 * @param originator
	 */
	virtual void OnUse(Entity* originator);

	/**
	 * Save data from this componennt to character XML
	 * @param doc the document to write data to
	 */
	virtual void UpdateXml(tinyxml2::XMLDocument* doc);

	/**
	 * Load base data for this component from character XML
	 * @param doc the document to read data from
	 */
	virtual void LoadFromXml(tinyxml2::XMLDocument* doc);

	/**
	 * Serialize component
	 * @param outBitStream Raknet output bitstream
	 * @param isConstruction Boolean representing construction state
	*/
	virtual void Serialize(RakNet::BitStream* outBitStream, bool isConstruction);

protected:
	/**
	 * Explicitly define default move and move-assignment constructors as
	 * definition of virtual destructor prevents their implicit generation
	*/
	Component(const Component& other) = delete; 					// Copy constructor (deleted)
	Component(Component&& other) = default; 						// Move constructor
	virtual Component& operator=(const Component& other) = delete; 	// Copy assignment constructor (deleted)
	virtual Component& operator=(Component&& other) = default; 		// Move assignment constructor

	/**
	 * The entity that owns this component
	 */
	LWOOBJID m_Parent;

	/**
	 * Helper function to check static assertions for derived classes
	*/
	template <typename CType>
	constexpr static void CheckComponentAssertions() {
		//constexpr auto componentName = 
		static_assert(std::is_move_constructible<CType>(), "Component is not move-constructible!");
		static_assert(std::is_move_assignable<CType>(), "Component is not move-assignable!");
	}
};
