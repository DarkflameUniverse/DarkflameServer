#pragma once

#include "Entity.h"

/**
 * Extended Entity for player data and behavior.
 *
 * Contains properties only a player entity would require, like associated SystemAddress and User.
 *
 * Keeps track of which entities are observed by this user for ghosting.
 */
class Player final : public Entity
{
public:
	explicit Player(const LWOOBJID& objectID, EntityInfo info, User* user, Entity* parentEntity = nullptr);

	/**
	 * Getters
	 */

	User* GetParentUser() const override { return m_ParentUser; };

	std::map<LWOOBJID, Loot::Info>& GetDroppedLoot() { return m_DroppedLoot; };

	uint64_t GetDroppedCoins() const { return m_DroppedCoins; };

	/**
	 * Setters
	 */

	void SetDroppedCoins(const uint64_t value) { m_DroppedCoins = value; };
private:
	User* m_ParentUser;

	std::map<LWOOBJID, Loot::Info> m_DroppedLoot;

	uint64_t m_DroppedCoins;
};
