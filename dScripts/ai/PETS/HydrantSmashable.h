#pragma once
#include "CppScripts.h"

class HydrantSmashable : public CppScripts::Script
{
public:
	void OnDie(Entity* self, Entity* killer) override;
private:
	LOT HYDRANT_BROKEN = 7328;
};
