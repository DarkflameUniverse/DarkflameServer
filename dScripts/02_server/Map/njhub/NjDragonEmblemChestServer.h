#pragma once
#include "CppScripts.h"

class NjDragonEmblemChestServer : public CppScripts::Script {
	void OnUse(Entity* self, Entity* user) override;
};
