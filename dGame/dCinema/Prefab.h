#pragma once

#include "dCommonVars.h"
#include "Entity.h"
#include <unordered_map>
#include <cstdint>

namespace Cinema
{

/**
 * @brief A prefab is a collection of objects that can be placed in the world together.
 * 
 * Can be saved and loaded from XML.
 */
class Prefab
{
public:
	Prefab() = default;

	~Prefab() = default;

	/**
	 * @brief Adds an object to the prefab.
	 * 
	 * @param lot The LOT of the object to add.
	 * @param position The position of the object to add.
	 * @param rotation The rotation of the object to add.
	 * @param scale The scale of the object to add.
	 * 
	 * @return The ID of the object that was added.
	 */
	size_t AddObject(LOT lot, NiPoint3 position, NiQuaternion rotation, float scale = 1.0f);

	/**
	 * @brief Adds an effect to the prefab.
	 * 
	 * @param objectID The ID of the object to add the effect to.
	 * @param effectID The ID of the effect to add.
	 */
	void AddEffect(size_t objectID, int32_t effectID);

	/**
	 * @brief Removes an object from the prefab.
	 * 
	 * @param objectID The ID of the object to remove.
	 */
	void RemoveObject(size_t objectID);

	/**
	 * @brief Loads a prefab from the given file.
	 * 
	 * @param file The file to load the prefab from.
	 * @return The prefab that was loaded.
	 */
	static const Prefab& LoadFromFile(std::string file);

	/**
	 * @brief Saves the prefab to the given file.
	 * 
	 * @param file The file to save the prefab to.
	 */
	void SaveToFile(std::string file);

	/**
	 * @brief Instantiates the prefab in the world.
	 * 
	 * @param position The position to instantiate the prefab at.
	 * @param scale The scale to instantiate the prefab with.
	 * 
	 * @return The ID of the instance that was created.
	 */
	size_t Instantiate(NiPoint3 position, float scale = 1.0f) const;

	/**
	 * @brief Get the list of entities in the instance with the given ID.
	 * 
	 * @param instanceID The ID of the instance to get the entities for.
	 * 
	 * @return The list of entities in the instance.
	 */
	static const std::vector<LWOOBJID>& GetEntities(size_t instanceID);

	/**
	 * @brief Destroys the instance with the given ID.
	 * 
	 * @param instanceID The ID of the instance to destroy.
	 */
	static void DestroyInstance(size_t instanceID);
	

private:
	struct Piece
	{
		LOT m_Lot;
		NiPoint3 m_Position;
		NiQuaternion m_Rotation;
		float m_Scale;
		std::vector<int32_t> m_Effects;
	};

	std::unordered_map<size_t, Piece> m_Pieces;
};

}