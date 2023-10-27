#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <functional>

#include "dCommonVars.h"

namespace Cinema
{

/**
 * @brief A collection of variables and other information to be shared between scenes and acts.
 */
struct Play
{
public:
	/**
	 * @brief The variables in the collection.
	 */
	std::unordered_map<std::string, std::string> variables;

	/**
	 * @brief Associated player which is watching the scene.
	 */
	LWOOBJID player;

	/**
	 * @brief A set of all entities involved in the scene.
	 */
	std::unordered_set<LWOOBJID> entities;

	/**
	 * @brief The scene that is currently being performed.
	 */
	class Scene* scene;

	/**
	 * @brief Conclude the play. Accepting/completing missions. This will not remove the entities.
	 */
	void Conclude();

	/**
	 * @brief Setup a check for if the audience is still present.
	 * 
	 * If the audience is no longer present, the play will be concluded and the entities will be removed.
	 */
	void SetupCheckForAudience();

	/**
	 * @brief Clean up the play.
	 */
	void CleanUp();

	/**
	 * @brief Setup a barrier. A callback is given when a signal is sent with the given barrier name.
	 * 
	 * @param barrier The name of the barrier.
	 * @param callback The callback to call when the barrier is signaled.
	 */
	void SetupBarrier(const std::string& barrier, std::function<void()> callback);

	/**
	 * @brief Signal a barrier.
	 * 
	 * @param barrier The name of the barrier to signal.
	 */
	void SignalBarrier(const std::string& barrier);

private:
	/**
	 * @brief Check if the audience is still present.
	 */
	void CheckForAudience();

	bool m_CheckForAudience = false;

	std::unordered_map<std::string, std::vector<std::function<void()>>> m_Barriers;

};

}