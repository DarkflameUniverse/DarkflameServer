#pragma once
#include "CppScripts.h"

class BaseEnemyMech : public CppScripts::Script
{
public:
	void OnDie(Entity* self, Entity* killer) override;
protected:
	LOT qbTurretLOT = 6254;
};

