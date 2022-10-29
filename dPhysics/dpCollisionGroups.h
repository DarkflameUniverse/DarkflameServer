#pragma once

#include <cstdint>

/*
 * Collision Groups
 */

enum eCollisionGroup : uint8_t
{
	COLLISION_GROUP_ALL = 0 << 0,
	COLLISION_GROUP_NEUTRAL = 1 << 0,
	COLLISION_GROUP_FRIENDLY = 1 << 1,
	COLLISION_GROUP_ENEMY = 1 << 2,
	COLLISION_GROUP_DYNAMIC = 1 << 3,
};
