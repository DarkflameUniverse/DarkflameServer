#pragma once
#include "CppScripts.h"

class FvDragonInstanceServer : public CppScripts::Script {
public:
	void OnPlayerLoaded(Entity* self, Entity* player) override;
};
