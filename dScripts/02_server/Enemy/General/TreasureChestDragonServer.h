#pragma once
#include "CppScripts.h"

class TreasureChestDragonServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
};
