#pragma once

#include "Entity.h"

class Player final : public Entity
{
public:
	explicit Player(const LWOOBJID& objectID, EntityInfo info, User* user, Entity* parentEntity = nullptr);
};
