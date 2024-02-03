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

	User* GetParentUser() const override { return m_ParentUser; };
private:

	User* m_ParentUser;
};
