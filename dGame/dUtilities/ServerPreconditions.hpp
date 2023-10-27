#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "dCommonVars.h"
#include "Preconditions.h"

class Entity;

/**
 * @brief Contains a series of additions to the server-side checks for whether or not an entity 
 * 	      is shown to the client or not.
 */
namespace ServerPreconditions
{

extern std::unordered_map<LOT, std::vector<std::pair<bool, PreconditionExpression>>> m_Preconditions;

extern std::unordered_map<LWOOBJID, LWOOBJID> m_SoloActors;

extern std::unordered_map<LWOOBJID, std::unordered_set<LWOOBJID>> m_ExcludeForPlayer;

/**
 * @brief Loads the preconditions from the given file.
 * 
 * @param file The file to load the preconditions from.
 * 
 * @section Example
 * <Preconditions>
 *   <Entity lot="2097254">
 *       <Precondition>1006</Precondition>
 *   </Entity>
 *   <Entity lot="12261">
 *       <Precondition not="1">1006</Precondition>
 *  </Entity>
 * </Preconditions>
 */
void LoadPreconditions(std::string file);

/**
 * @brief Checks the additional server-side preconditions for the given entity.
 * 
 * @param target The entity to check the preconditions for.
 * @param entity The entity to check the preconditions against (usually the player).
 * 
 * @return Whether or not the entity passes the preconditions.
 */
bool CheckPreconditions(Entity* target, Entity* entity);

/**
 * @brief Checks if a given entity is a solo actor.
 * 
 * Solo actors are entities that are only shown to the client if they are acting for the player.
 */
bool IsSoloActor(LWOOBJID actor);

/**
 * @brief Checks if a given entity is acting for another entity.
 * 
 * @param actor The entity to check if it is acting for another entity.
 * @param target The entity to check if the actor is acting for (usually the player).
 * 
 * @return Whether or not the actor is acting for the target.
 */
bool IsActingFor(LWOOBJID actor, LWOOBJID target);

/**
 * @brief Adds an entity to the list of solo actors.
 * 
 * @param actor The entity to add to the list of solo actors.
 * @param target The entity to add the actor to the list of solo actors for (usually the player).
 */
void AddSoloActor(LWOOBJID actor, LWOOBJID target);

/**
 * @brief Removes an entity from the list of solo actors.
 * 
 * @param actor The entity to remove from the list of solo actors.
 */
void RemoveSoloActor(LWOOBJID actor);

/**
 * @brief Adds an entity to the list of entities to exclude for another entity.
 * 
 * @param player The entity to exclude the target for (usually the player).
 * @param target The entity to exclude for the player.
 */
void AddExcludeFor(LWOOBJID player, LWOOBJID target);

/**
 * @brief Removes an entity from the list of entities to exclude for another entity.
 * 
 * @param player The entity to remove the target from the list of entities to exclude for (usually the player).
 * @param target The entity to remove from the list of entities to exclude for the player.
 */
void RemoveExcludeFor(LWOOBJID player, LWOOBJID target);

/**
 * @brief Checks if an entity is excluded for another entity.
 * 
 * @param player The entity to check if the target is excluded for (usually the player).
 * @param target The entity to check if it is excluded for the player.
 * 
 * @return Whether or not the target is excluded for the player.
 */
bool IsExcludedFor(LWOOBJID player, LWOOBJID target);

}