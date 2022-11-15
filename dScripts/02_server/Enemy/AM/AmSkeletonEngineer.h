#pragma once
#include "CppScripts.h"
#include "EnemyNjBuff.h"

class AmSkeletonEngineer : public EnemyNjBuff
{
public:
	void OnHit(Entity* self, Entity* attacker) override;
};
