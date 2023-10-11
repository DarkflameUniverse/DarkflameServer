#pragma once
#include "CppScripts.h"

class SpawnSkeletonOnDeath : public CppScripts::Script
{
public:
	void OnDie(Entity* self, Entity* killer) override;
};
