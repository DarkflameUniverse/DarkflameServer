#pragma once
#include "CppScripts.h"

class MonCoreSmashableDoors : public CppScripts::Script {
public:
	void OnDie(Entity* self, Entity* killer) override;
};

