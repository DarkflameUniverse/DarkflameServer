#pragma once

#include "Prefab.h"
#include "Recorder.h"
#include "Preconditions.h"

namespace Cinema
{

/**
 * @brief A scene is a collection of prefabs, npcs and behaviors that can be loaded into the world. 
 */
class Scene
{
public:
	Scene() = default;

	/**
	 * @brief Adds an object to the scene.
	 * 
	 * @param lot The LOT of the object to add.
	 * @param position The position of the object to add.
	 * @param rotation The rotation of the object to add.
	 */
	void AddObject(LOT lot, NiPoint3 position, NiQuaternion rotation);

	/**
	 * @brief Adds a prefab to the scene.
	 * 
	 * @param prefab The prefab to add.
	 * @param position The position to add the prefab at.
	 */
	void AddPrefab(const Prefab& prefab, NiPoint3 position);

	/**
	 * @brief Adds an NPC to the scene.
	 * 
	 * @param npc The NPC to add.
	 * @param act The act to set for the NPC.
	 */
	void AddNPC(LOT npc, const std::string& name, Recording::Recorder* act);

	/**
	 * @brief Set up the scene to be acted when a player enters the theater and meets the preconditions.
	 */
	void Rehearse();

	/**
	 * @brief Conclude the scene for a given player.
	 * 
	 * @param player The player to conclude the scene for (not nullptr).
	 */
	void Conclude(Entity* player);

	/**
	 * @brief Checks if a given player is within the bounds of the scene.
	 * 
	 * @param player The player to check.
	 */
	bool IsPlayerInBounds(Entity* player) const;

	/**
	 * @brief Checks if a given player is within the showing distance of the scene.
	 * 
	 * @param player The player to check.
	 */
	bool IsPlayerInShowingDistance(Entity* player) const;

	/**
	 * @brief Checks if a given player is within the maximum showing distance of the scene.
	 * 
	 * @param player The player to check.
	 */
	bool IsPlayerInMaximumShowingDistance(Entity* player) const;

	/**
	 * @brief Act the scene.
	 * 
	 * @param player The player to act the scene for (or nullptr to act for all players).
	 * @return The variables that were set by the scene.
	 */
	Play* Act(Entity* player);
	
	/**
	 * @brief Loads a scene from the given file.
	 * 
	 * @param file The file to load the scene from.
	 * @return The scene that was loaded.
	 */
	static Scene& LoadFromFile(std::string file);

	/**
	 * @brief Automatically loads the scenes for a given zone.
	 * 
	 * @param zone The zone to load the scenes for.
	 */
	static void AutoLoadScenesForZone(LWOMAPID zone);

private:
	void CheckForShowings();

	void CheckTicket(Entity* player);

	std::vector<std::pair<LOT, std::pair<NiPoint3, NiQuaternion>>> m_Objects;
	std::vector<std::pair<Prefab, NiPoint3>> m_Prefabs;
	std::vector<std::pair<LOT, std::pair<Recording::Recorder*, std::string>>> m_NPCs;

	NiPoint3 m_Center;
	float m_Bounds = 0.0f;
	float m_ShowingDistance = 0.0f;
	float m_MaximumShowingDistance = 0.0f;
	float m_ChanceToPlay = 1.0f;
	bool m_Repeatable = true;

	std::vector<std::pair<PreconditionExpression, bool>> m_Preconditions;

	int32_t m_AcceptMission = 0;
	int32_t m_CompleteMission = 0;

	std::unordered_set<LWOOBJID> m_Audience;
	std::unordered_set<LWOOBJID> m_HasBeenOutside;

	std::unordered_set<LWOOBJID> m_VisitedPlayers;

	static std::unordered_map<std::string, Scene> m_Scenes;

	static void CommandRecordAct(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandRecordStart(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandRecordStop(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandRecordSave(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandRecordLoad(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandPrefabSpawn(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandPrefabDestroy(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandSceneAct(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandSceneSetup(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandCompanion(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandCinematic(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	static void CommandGhostReference(Entity* entity, const SystemAddress& sysAddr, const std::string args);
};

} // namespace Cinema
